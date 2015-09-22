#include "../base/SRC_FIRST.hpp"
#include "path_text_element.hpp"
#include "overlay_renderer.hpp"

namespace graphics
{
  PathTextElement::PathTextElement(Params const & p)
    : TextElement(p),
      m_glyphLayout(p.m_glyphCache,
        p.m_fontDesc,
        p.m_pts,
        p.m_ptsCount,
        visText(),
        p.m_fullLength,
        p.m_pathOffset,
        p.m_position)
  {
    setPivot(m_glyphLayout.pivot());
    setIsValid((m_glyphLayout.firstVisible() == 0) && (m_glyphLayout.lastVisible() == visText().size()));
  }

  PathTextElement::PathTextElement(PathTextElement const & src, math::Matrix<double, 3, 3> const & m)
    : TextElement(src),
      m_glyphLayout(src.m_glyphLayout, m)
  {
    setPivot(m_glyphLayout.pivot());
    setIsValid((m_glyphLayout.firstVisible() == 0) && (m_glyphLayout.lastVisible() == visText().size()));
  }

  vector<m2::AnyRectD> const & PathTextElement::boundRects() const
  {
    if (isDirtyRect())
    {
      m_boundRects.clear();
      m_boundRects.reserve(m_glyphLayout.boundRects().size());

      for (unsigned i = 0; i < m_glyphLayout.boundRects().size(); ++i)
        m_boundRects.push_back(m_glyphLayout.boundRects()[i]);

//      for (unsigned i = 0; i < m_boundRects.size(); ++i)
//        m_boundRects[i] = m2::Inflate(m_boundRects[i], m2::PointD(10, 10));
  //    m_boundRects[i].m2::Inflate(m2::PointD(40, 2)); //< to create more sparse street names structure
      setIsDirtyRect(false);
    }

    return m_boundRects;
  }

  void PathTextElement::draw(OverlayRenderer * screen, math::Matrix<double, 3, 3> const & m) const
  {
    if (screen->isDebugging())
    {
      graphics::Color c(255, 255, 255, 32);

      if (isFrozen())
        c = graphics::Color(0, 0, 255, 64);
      if (isNeedRedraw())
        c = graphics::Color(255, 0, 0, 64);

      screen->drawRectangle(roughBoundRect(), graphics::Color(255, 255, 0, 64), depth());

      for (unsigned i = 0; i < boundRects().size(); ++i)
        screen->drawRectangle(boundRects()[i], c, depth());
    }

    if (!isNeedRedraw() || !isVisible() || !isValid())
      return;

    graphics::FontDesc desc = m_fontDesc;

    if (desc.m_isMasked)
    {
      drawTextImpl(m_glyphLayout, screen, m, false, false, desc, depth());
      desc.m_isMasked = false;
    }

    drawTextImpl(m_glyphLayout, screen, m, false, false, desc, depth());
  }

  void PathTextElement::setPivot(m2::PointD const & pivot)
  {
    TextElement::setPivot(pivot);
    m_glyphLayout.setPivot(pivot);
  }

  OverlayElement * PathTextElement::clone(math::Matrix<double, 3, 3> const & m) const
  {
    return new PathTextElement(*this, m);
  }
}
