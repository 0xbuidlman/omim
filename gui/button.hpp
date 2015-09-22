#pragma once

#include "element.hpp"
#include "text_view.hpp"

#include "../std/function.hpp"
#include "../std/string.hpp"
#include "../std/scoped_ptr.hpp"

namespace graphics
{
  class OverlayElement;

  namespace gl
  {
    class OverlayRenderer;
  }
}

namespace gui
{
  class Button : public Element
  {
  public:

    typedef function<void (Element const *)> TOnClickListener;

  private:

    TOnClickListener m_OnClickListener;

    unsigned m_minWidth;
    unsigned m_minHeight;

    scoped_ptr<TextView> m_textView;

    mutable vector<m2::AnyRectD> m_boundRects;

  public:

    struct Params : Element::Params
    {
      unsigned m_minWidth;
      unsigned m_minHeight;
      string m_text;
    };

    Button(Params const & p);

    bool onTapStarted(m2::PointD const & pt);
    bool onTapMoved(m2::PointD const & pt);
    bool onTapEnded(m2::PointD const & pt);
    bool onTapCancelled(m2::PointD const & pt);

    void setOnClickListener(TOnClickListener const & l);

    void setPivot(m2::PointD const & pv);
    void setFont(EState state, graphics::FontDesc const & font);
    void setColor(EState state, graphics::Color const & c);

    void setText(string const & text);
    string const & text() const;

    void setMinWidth(unsigned minWidthInDIP);
    unsigned minWidth() const;

    void setMinHeight(unsigned minHeightInDIP);
    unsigned minHeight() const;

    void setController(Controller * controller);

    /// Inherited from OverlayElement
    /// @{

    vector<m2::AnyRectD> const & boundRects() const;
    void draw(graphics::gl::OverlayRenderer * r, math::Matrix<double, 3, 3> const & m) const;

    /// @}
  };
}
