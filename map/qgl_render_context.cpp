#include "../base/SRC_FIRST.hpp"

#include "qgl_render_context.hpp"

#include "../base/assert.hpp"
#include "../base/logging.hpp"

#include <QtOpenGL/QGLContext>
#include <QtOpenGL/QGLWidget>
#include <QtOpenGL/QGLFramebufferObject>


namespace qt
{
  namespace gl
  {
    struct null_deleter
    {
      template <typename T> void operator()(T*) {}
    };

    /// Create compatible render context
    RenderContext::RenderContext(QGLWidget * widget)
    {
      /// Dirty hack, but we'll use it with caution, I promise.
      m_context = shared_ptr<QGLContext>(const_cast<QGLContext*>(widget->context()), null_deleter());
    }

    void RenderContext::makeCurrent()
    {
      m_context->makeCurrent();
      graphics::gl::RenderContext::initParams();
    }

    shared_ptr<graphics::gl::RenderContext> RenderContext::createShared()
    {
      return shared_ptr<graphics::gl::RenderContext>(new RenderContext(this));
    }

    void RenderContext::endThreadDrawing()
    {
      m_context.reset();
      graphics::gl::RenderContext::endThreadDrawing();
    }

    RenderContext::RenderContext(RenderContext * renderContext)
    {
      QGLFormat const format = renderContext->context()->format();
      m_parent = make_shared_ptr(new QWidget());
      m_context = shared_ptr<QGLContext>(new QGLContext(format, m_parent.get()));
      bool sharedContextCreated = m_context->create(renderContext->context().get());
      bool isSharing = m_context->isSharing();
      ASSERT(sharedContextCreated && isSharing, ("cannot create shared opengl context"));
      if (!sharedContextCreated || !isSharing)
        m_context.reset();
    }

    RenderContext::~RenderContext()
    {
      m_context.reset();
      m_parent.reset();
    }

    shared_ptr<QGLContext> RenderContext::context() const
    {
      return m_context;
    }
  }
}
