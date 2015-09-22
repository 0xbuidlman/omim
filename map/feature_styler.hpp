#pragma once

#include "../indexer/drawing_rules.hpp"

#include "../geometry/rect2d.hpp"

#include "../std/vector.hpp"

class FeatureType;

namespace graphics
{
  class GlyphCache;
}

class ScreenBase;

namespace di
{
  struct DrawRule
  {
    drule::BaseRule const * m_rule;
    double m_depth;
    bool m_transparent;

    DrawRule() : m_rule(0) {}
    DrawRule(drule::BaseRule const * p,
             double d,
             bool tr)
      : m_rule(p),
        m_depth(d),
        m_transparent(tr)
    {}

    uint32_t GetID(size_t threadSlot) const;
    void SetID(size_t threadSlot, uint32_t id) const;
  };

  struct FeatureStyler
  {
    FeatureStyler(FeatureType const & f,
                  int const zoom,
                  double const visualScale,
                  graphics::GlyphCache * glyphCache,
                  ScreenBase const * convertor,
                  m2::RectD const * rect);

    typedef buffer_vector<di::DrawRule, 8> StylesContainerT;
    StylesContainerT m_rules;

    bool m_isCoastline;
    bool m_hasLineStyles;
    bool m_hasPointStyles;
    bool m_hasPathText;
    int m_geometryType;

    double m_visualScale;

    string m_primaryText;
    string m_secondaryText;
    string m_refText;

    typedef buffer_vector<double, 16> ClipIntervalsT;
    ClipIntervalsT m_intervals;

    typedef buffer_vector<double, 8> TextOffsetsT;
    TextOffsetsT m_offsets;

    uint8_t m_fontSize;

    graphics::GlyphCache * m_glyphCache;
    ScreenBase const * m_convertor;
    m2::RectD const * m_rect;

    double m_popRank;

    bool IsEmpty() const;

    string const GetPathName() const;

    bool FilterTextSize(drule::BaseRule const * pRule) const;

  private:
    uint8_t GetTextFontSize(drule::BaseRule const * pRule) const;
    void LayoutTexts(double pathLength);
  };
}
