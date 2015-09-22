#pragma once

#include "overlay_element.hpp"
#include "icon.hpp"

namespace graphics
{
  class Skin;

  class SymbolElement : public OverlayElement
  {
  private:

    Icon::Info m_info;
    m2::RectU m_symbolRect;

    mutable vector<m2::AnyRectD> m_boundRects;

    m2::AnyRectD const boundRect() const;

  public:

    typedef OverlayElement base_t;

    struct Params : public base_t::Params
    {
      Icon::Info m_info;
      m2::RectU m_symbolRect;
      OverlayRenderer * m_renderer;
      Params();
    };

    SymbolElement(Params const & p);
    SymbolElement(SymbolElement const & se, math::Matrix<double, 3, 3> const & m);

    vector<m2::AnyRectD> const & boundRects() const;
    void draw(OverlayRenderer * s, math::Matrix<double, 3, 3> const & m) const;

    uint32_t resID() const;

    OverlayElement * clone(math::Matrix<double, 3, 3> const & m) const;

    bool hasSharpGeometry() const;
  };
}
