#include "glyph_layout.hpp"
#include "font_desc.hpp"
#include "resource.hpp"

#include "../base/logging.hpp"
#include "../base/math.hpp"
#include "../std/sstream.hpp"

#include "../geometry/angles.hpp"
#include "../geometry/any_rect2d.hpp"
#include "../base/thread.hpp"

namespace graphics
{
  double GlyphLayout::getKerning(GlyphLayoutElem const & prevElem,
                                 GlyphMetrics const & prevMetrics,
                                 GlyphLayoutElem const & curElem,
                                 GlyphMetrics const & curMetrics)
  {
    double res = 0;
    /// check, whether we should offset this symbol slightly
    /// not to overlap the previous symbol

    m2::AnyRectD prevSymRectAA(
          prevElem.m_pt.Move(prevMetrics.m_height,
                            -prevElem.m_angle.cos(),
                             prevElem.m_angle.sin()), //< moving by angle = prevElem.m_angle - math::pi / 2
          prevElem.m_angle,
          m2::RectD(prevMetrics.m_xOffset,
                    prevMetrics.m_yOffset,
                    prevMetrics.m_xOffset + prevMetrics.m_width,
                    prevMetrics.m_yOffset + prevMetrics.m_height));

    m2::AnyRectD curSymRectAA(
          curElem.m_pt.Move(curMetrics.m_height,
                           -curElem.m_angle.cos(),
                            curElem.m_angle.sin()), //< moving by angle = curElem.m_angle - math::pi / 2
          curElem.m_angle,
          m2::RectD(curMetrics.m_xOffset,
                    curMetrics.m_yOffset,
                    curMetrics.m_xOffset + curMetrics.m_width,
                    curMetrics.m_yOffset + curMetrics.m_height)
          );

    if (prevElem.m_angle.val() == curElem.m_angle.val())
      return res;

    //m2::RectD prevLocalRect = prevSymRectAA.GetLocalRect();
    m2::PointD pts[4];
    prevSymRectAA.GetGlobalPoints(pts);
    curSymRectAA.ConvertTo(pts, 4);

    m2::RectD prevRectInCurSym(pts[0].x, pts[0].y, pts[0].x, pts[0].y);
    prevRectInCurSym.Add(pts[1]);
    prevRectInCurSym.Add(pts[2]);
    prevRectInCurSym.Add(pts[3]);

    m2::RectD const & curSymRect = curSymRectAA.GetLocalRect();

    if (curSymRect.minX() < prevRectInCurSym.maxX())
      res = prevRectInCurSym.maxX() - curSymRect.minX();

    return res;
  }

  GlyphLayout::GlyphLayout()
  {}

  GlyphLayout::GlyphLayout(GlyphCache * glyphCache,
                           FontDesc const & fontDesc,
                           m2::PointD const & pt,
                           strings::UniString const & visText,
                           graphics::EPosition pos)
    : m_firstVisible(0),
      m_lastVisible(visText.size()),
      m_fontDesc(fontDesc),
      m_pivot(pt),
      m_offset(0, 0),
      m_textLength(0),
      m_textOffset(0)
  {
    if (!m_fontDesc.IsValid())
      return;

    size_t const cnt = visText.size();
    ASSERT_GREATER(cnt, 0, ());

    m_entries.resize(cnt);
    m_metrics.resize(cnt);

    m2::RectD boundRect;
    m2::PointD curPt(0, 0);

    bool isFirst = true;

    for (size_t i = 0; i < visText.size(); ++i)
    {
      GlyphKey glyphKey(visText[i],
                        fontDesc.m_size,
                        //fontDesc.m_isMasked,
                        false, //< calculating glyph positions using unmasked glyphs.
                        fontDesc.m_color);

      GlyphMetrics const m = glyphCache->getGlyphMetrics(glyphKey);

      m_textLength += m.m_xAdvance;

      if (isFirst)
      {
        boundRect = m2::RectD(m.m_xOffset,
                             -m.m_yOffset,
                              m.m_xOffset,
                             -m.m_yOffset);
        isFirst = false;
      }
      else
        boundRect.Add(m2::PointD(m.m_xOffset + curPt.x, -m.m_yOffset + curPt.y));

      boundRect.Add(m2::PointD(m.m_xOffset + m.m_width,
                             -(m.m_yOffset + m.m_height)) + curPt);

      GlyphLayoutElem elem;
      elem.m_sym = visText[i];
      elem.m_angle = 0;
      elem.m_pt = curPt;
      m_entries[i] = elem;
      m_metrics[i] = m;

      curPt += m2::PointD(m.m_xAdvance, m.m_yAdvance);
    }

    boundRect.Inflate(2, 2);

    m2::PointD ptOffs(-boundRect.SizeX() / 2 - boundRect.minX(),
                      -boundRect.SizeY() / 2 - boundRect.minY());

    // adjusting according to position
    if (pos & EPosLeft)
      ptOffs += m2::PointD(-boundRect.SizeX() / 2, 0);

    if (pos & EPosRight)
      ptOffs += m2::PointD(boundRect.SizeX() / 2, 0);

    if (pos & EPosAbove)
      ptOffs += m2::PointD(0, -boundRect.SizeY() / 2);

    if (pos & EPosUnder)
      ptOffs += m2::PointD(0, boundRect.SizeY() / 2);

    for (unsigned i = 0; i < m_entries.size(); ++i)
      m_entries[i].m_pt += ptOffs;

    boundRect.Offset(ptOffs);

    m_boundRects.push_back(m2::AnyRectD(boundRect));
  }

  GlyphLayout::GlyphLayout(GlyphLayout const & src,
                           math::Matrix<double, 3, 3> const & m)
    : m_firstVisible(0),
      m_lastVisible(0),
      m_path(src.m_path, m),
      m_visText(src.m_visText),
      m_fontDesc(src.m_fontDesc),
      m_metrics(src.m_metrics),
      m_pivot(0, 0),
      m_offset(0, 0),
      m_textLength(src.m_textLength)
  {
    if (!m_fontDesc.IsValid())
      return;

    m_boundRects.push_back(m2::AnyRectD(m2::RectD(0, 0, 0, 0)));

    m_textOffset = (m2::PointD(0, src.m_textOffset) * m).Length(m2::PointD(0, 0) * m);

    // if isReverse flag is changed, recalculate m_textOffset
    if (src.m_path.isReverse() != m_path.isReverse())
      m_textOffset = m_path.fullLength() - m_textOffset - m_textLength;

    recalcAlongPath();
  }

  GlyphLayout::GlyphLayout(GlyphCache * glyphCache,
                           FontDesc const & fontDesc,
                           m2::PointD const * pts,
                           size_t ptsCount,
                           strings::UniString const & visText,
                           double fullLength,
                           double pathOffset,
                           double textOffset)
    : m_firstVisible(0),
      m_lastVisible(0),
      m_path(pts, ptsCount, fullLength, pathOffset),
      m_visText(visText),
      m_fontDesc(fontDesc),
      m_pivot(0, 0),
      m_offset(0, 0),
      m_textLength(0),
      m_textOffset(textOffset)
  {
    if (!m_fontDesc.IsValid())
      return;

    m_boundRects.push_back(m2::AnyRectD(m2::RectD(0, 0, 0, 0)));

    size_t const cnt = m_visText.size();
    m_metrics.resize(cnt);

    for (size_t i = 0; i < cnt; ++i)
    {
      GlyphKey key(visText[i],
                   m_fontDesc.m_size,
                   false, // calculating glyph positions using the unmasked glyphs.
                   graphics::Color(0, 0, 0, 0));
      m_metrics[i] = glyphCache->getGlyphMetrics(key);
      m_textLength += m_metrics[i].m_xAdvance;
    }

    // if was reversed - recalculate m_textOffset
    if (m_path.isReverse())
      m_textOffset = m_path.fullLength() - m_textOffset - m_textLength;

    recalcAlongPath();
  }

  void GlyphLayout::recalcAlongPath()
  {
    size_t const count = m_visText.size();
    if (count == 0 || m_path.fullLength() < m_textLength)
      return;

    m_entries.resize(count);
    for (size_t i = 0; i < count; ++i)
      m_entries[i].m_sym = m_visText[i];

    PathPoint arrPathStart = m_path.front();

    // Offset of the text from path's start.
    // In normal behaviour it should be always > 0,
    // but now we do scale tiles for the fixed layout.
    double offset = m_textOffset - m_path.pathOffset();
    if (offset < 0.0)
    {
      /// @todo Try to fix this heuristic.
      if (offset > -3.0)
        offset = 0.0;
      else
        return;
    }

    // find first visible glyph
    size_t symPos = 0;
    //while (offset < 0 &&  symPos < count)
    //  offset += m_metrics[symPos++].m_xAdvance;

    PathPoint glyphStartPt = m_path.offsetPoint(arrPathStart, offset);

    /// @todo Calculate better pivot (invariant point when scaling and rotating text).
    m_pivot = glyphStartPt.m_pt;

    m_firstVisible = symPos;

    GlyphLayoutElem prevElem; // previous glyph, to compute kerning from
    GlyphMetrics prevMetrics;
    bool hasPrevElem = false;

    // calculate base line offset
    double const blOffset = 2 - m_fontDesc.m_size / 2;

    for (; symPos < count; ++symPos)
    {
      if (glyphStartPt.m_i == -1)
      {
        //LOG(LWARNING, ("Can't find glyph", symPos, count));
        return;
      }

      GlyphMetrics const & metrics = m_metrics[symPos];
      GlyphLayoutElem & entry = m_entries[symPos];

      // full advance, including possible kerning.
      double fullGlyphAdvance = metrics.m_xAdvance;

      if (metrics.m_width != 0)
      {
        double fullKern = 0;
        bool pathIsTooBended = false;

        int i = 0;
        for (; i < 100; ++i)
        {
          PivotPoint pivotPt = m_path.findPivotPoint(glyphStartPt, metrics, fullKern);

          if (pivotPt.m_pp.m_i == -1)
          {
            //LOG(LWARNING, ("Path text pivot error for glyph", symPos, count));
            return;
          }

          entry.m_angle = pivotPt.m_angle;
          double const centerOffset = metrics.m_xOffset + metrics.m_width / 2.0;
          entry.m_pt = pivotPt.m_pp.m_pt.Move(-centerOffset,
                                              entry.m_angle.sin(),
                                              entry.m_angle.cos());

          entry.m_pt = entry.m_pt.Move(blOffset,
                                       -entry.m_angle.cos(),
                                       entry.m_angle.sin());

          // check if angle between letters is too big
          if (hasPrevElem && (ang::GetShortestDistance(prevElem.m_angle.m_val, entry.m_angle.m_val) > 0.8))
          {
            pathIsTooBended = true;
            break;
          }

          // check whether we should "kern"
          double kern = 0.0;
          if (hasPrevElem)
          {
            kern = getKerning(prevElem, prevMetrics, entry, metrics);
            if (kern < 0.5)
              kern = 0.0;

            fullGlyphAdvance += kern;
            fullKern += kern;
          }
          if (kern == 0.0)
            break;
        }

        if (i == 100)
          LOG(LINFO, ("100 iteration on computing kerning exceeded. possibly infinite loop occured"));

        if (pathIsTooBended)
          break;

        // kerning should be computed for baseline centered glyph
        prevElem = entry;
        prevMetrics = metrics;
        hasPrevElem = true;

        // align to baseline
        //entry.m_pt = entry.m_pt.Move(blOffset - kernOffset, entry.m_angle - math::pi / 2);
      }
      else
      {
        if (symPos == m_firstVisible)
        {
          m_firstVisible = symPos + 1;
        }
        else
        {
          m_entries[symPos].m_angle = ang::AngleD();
          m_entries[symPos].m_pt = glyphStartPt.m_pt;
        }
      }

      glyphStartPt = m_path.offsetPoint(glyphStartPt, fullGlyphAdvance);

      m_lastVisible = symPos + 1;
    }

    // storing glyph coordinates relative to pivot point.
    for (unsigned i = m_firstVisible; i < m_lastVisible; ++i)
      m_entries[i].m_pt -= m_pivot;

    computeBoundRects();
  }

  void GlyphLayout::computeBoundRects()
  {
    map<double, m2::AnyRectD> rects;

    for (unsigned i = m_firstVisible; i < m_lastVisible; ++i)
    {
      if (m_metrics[i].m_width != 0)
      {
        ang::AngleD const & a = m_entries[i].m_angle;

        map<double, m2::AnyRectD>::iterator it = rects.find(a.val());

        m2::AnyRectD symRectAA(
              m_entries[i].m_pt.Move(m_metrics[i].m_height + m_metrics[i].m_yOffset, -a.cos(), a.sin()), //< moving by angle = m_entries[i].m_angle - math::pi / 2
              a,
              m2::RectD(m_metrics[i].m_xOffset,
                        0,
                        m_metrics[i].m_xOffset + m_metrics[i].m_width,
                        m_metrics[i].m_height
                        ));

        if (it == rects.end())
          rects[a.val()] = symRectAA;
        else
          rects[a.val()].Add(symRectAA);
      }
    }

    m_boundRects.clear();

    for (map<double, m2::AnyRectD>::const_iterator it = rects.begin(); it != rects.end(); ++it)
    {
      m2::AnyRectD r(it->second);
      m2::PointD zero = r.GlobalZero();

      double dx = zero.x - floor(zero.x);
      double dy = zero.y - floor(zero.y);

      r.Offset(m2::PointD(-dx, -dy));

      r.Offset(m_pivot);

      m_boundRects.push_back(r);
    }
  }

  size_t GlyphLayout::firstVisible() const
  {
    return m_firstVisible;
  }

  size_t GlyphLayout::lastVisible() const
  {
    return m_lastVisible;
  }

  buffer_vector<GlyphLayoutElem, 32> const & GlyphLayout::entries() const
  {
    return m_entries;
  }

  buffer_vector<m2::AnyRectD, 16> const & GlyphLayout::boundRects() const
  {
    return m_boundRects;
  }

  m2::PointD const & GlyphLayout::pivot() const
  {
    return m_pivot;
  }

  void GlyphLayout::setPivot(m2::PointD const & pivot)
  {
    for (unsigned i = 0; i < m_boundRects.size(); ++i)
      m_boundRects[i].Offset(pivot - m_pivot);

    m_pivot = pivot;
  }

  m2::PointD const & GlyphLayout::offset() const
  {
    return m_offset;
  }

  void GlyphLayout::setOffset(m2::PointD const & offset)
  {
    for (unsigned i = 0; i < m_boundRects.size(); ++i)
      m_boundRects[i].Offset(offset - m_offset);

    m_offset = offset;
  }

  graphics::FontDesc const & GlyphLayout::fontDesc() const
  {
    return m_fontDesc;
  }
}
