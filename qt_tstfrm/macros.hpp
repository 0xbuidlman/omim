#pragma once

#include "widgets.hpp"
#include "main_tester.hpp"
#include "tstwidgets.hpp"

#include "../../testing/testing.hpp"
#include "../../map/qgl_render_context.hpp"

#include <QtOpenGL/QGLContext>
#include <QtOpenGL/QGLPixelBuffer>
#include <QtGui/QApplication>

//#include <iostream>

template<class T, bool (T::*)(QKeyEvent *)>
struct key_event_fn_bind
{
  typedef T type;
};

template <class T, bool  (T::*)(QMouseEvent*)>
struct mouse_event_fn_bind
{
  typedef T type;
};

template <class T, void (T::*)()>
struct void_fn_bind
{
  typedef T type;
};

template <class T, class RC, void (T::*)(shared_ptr<RC>)>
struct init_with_context_fn_bind
{
  typedef T type;
};

template<class T, class U>
struct has_on_keypress
{
  static const bool value = false;
};

template<class T>
struct has_on_keypress<T, typename key_event_fn_bind<T, &T::OnKeyPress>::type>
{
  static const bool value = true;
};

template <class T, class U>
struct has_on_mousemove
{
  static const bool value = false;
};

template <class T>
struct has_on_mousemove<T, typename mouse_event_fn_bind<T, &T::OnMouseMove>::type >
{
  static const bool value = true;
};

template <class T, class U>
struct has_on_mousepress
{
  static bool const value = false;
};

template <class T>
struct has_on_mousepress<T, typename mouse_event_fn_bind<T, &T::OnMousePress>::type >
{
  static const bool value = true;
};

template <class T, class U>
struct has_init
{
  static bool const value = false;
};

template <class T>
struct has_init<T, typename void_fn_bind<T, &T::Init>::type >
{
  static const bool value = true;
};

template <class T, class U, class RC>
struct has_init_with_context
{
  static bool const value = false;
};

template <class T, class RC>
struct has_init_with_context<T, typename init_with_context_fn_bind<T, RC, &T::Init>::type, RC >
{
  static const bool value = true;
};

template <bool T>
struct bool_tag{};

template <typename TTest>
class GLTestWidget : public tst::GLDrawWidget
{
  TTest test;

  typedef tst::GLDrawWidget base_type;

public:

  virtual void DoDraw(shared_ptr<yg::gl::Screen> p)
  {
    p->beginFrame();
    p->clear(yg::gl::Screen::s_bgColor);
    test.DoDraw(p);
    p->endFrame();
  }
  virtual void DoResize(int, int)
  {
  }

  bool keyPressEventImpl(QKeyEvent * ev, bool_tag<true> const &)
  {
    return test.OnKeyPress(ev);
  }

  bool keyPressEventImpl(QKeyEvent *, bool_tag<false> const & )
  {
    return false;
  }

  virtual void keyPressEvent(QKeyEvent * ev)
  {
    if (keyPressEventImpl(ev, bool_tag<has_on_keypress<TTest, TTest>::value >()))
      repaint();
  }

  bool mousePressEventImpl(QMouseEvent * ev, bool_tag<true> const &)
  {
    return test.OnMousePress(ev);
  }

  bool mousePressEventImpl(QMouseEvent *, bool_tag<false> const &)
  {
    return false;
  }

  virtual void mousePressEvent(QMouseEvent * ev)
  {
    if (mousePressEventImpl(ev, bool_tag<has_on_mousepress<TTest, TTest>::value >()))
      repaint();
  }

  bool mouseMoveEventImpl(QMouseEvent * ev, bool_tag<true> const &)
  {
    return test.OnMouseMove(ev);
  }

  bool mouseMoveEventImpl(QMouseEvent *, bool_tag<false> const &)
  {
    return false;
  }

  void mouseMoveEvent(QMouseEvent * ev)
  {
    if (mouseMoveEventImpl(ev, bool_tag<has_on_mousemove<TTest, TTest>::value >()))
      repaint();
  }

  void InitImpl(bool_tag<true> const & )
  {
    test.Init();
  }

  void InitImpl(bool_tag<false> const & )
  {}

  void Init()
  {
    InitImpl(bool_tag<has_init<TTest, TTest>::value >());
  }

  void InitWithContextImpl(bool_tag<true> const &)
  {
    test.Init(shared_ptr<qt::gl::RenderContext>(new qt::gl::RenderContext(this)));
  }

  void InitWithContextImpl(bool_tag<false> const &)
  {}

  void initializeGL()
  {
    tst::GLDrawWidget::initializeGL();
    InitWithContextImpl(bool_tag<has_init_with_context<TTest, TTest, qt::gl::RenderContext>::value>());
  }
};

template <class Test> QWidget * create_widget()
{
  GLTestWidget<Test> * w = new GLTestWidget<Test>();
  w->Init();
  return w;
}

#define GL_TEST_START           \
  int argc = 0;                 \
  QApplication app(argc, 0);    \
  QGLWidget * buf = new QGLWidget(); \
  QGLContext ctx(QGLFormat::defaultFormat(), buf);      \
  ctx.create();   \
  ctx.makeCurrent();

#define UNIT_TEST_GL(name)\
  void UnitTestGL_##name();\
  TestRegister g_TestRegister_##name("Test::"#name, __FILE__, &UnitTestGL_##name);\
  void UnitTestGL_##name()\
  {\
    tst::BaseTester tester;\
    tester.Run(#name, &create_widget<name>);\
  }
