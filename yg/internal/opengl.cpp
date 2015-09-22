#include "opengl.hpp"

#include "../../base/logging.hpp"
#include "../../base/string_utils.hpp"
#include "../../base/stl_add.hpp"

#include "../../std/bind.hpp"

#ifdef OMIM_OS_BADA
  #include <FBaseSys.h>
#endif


namespace yg
{
  namespace gl
  {
    platform_unsupported::platform_unsupported(char const * reason)
      : m_reason(reason)
    {}

    platform_unsupported::~platform_unsupported() throw()
    {}

    char const * platform_unsupported::what() const throw()
    {
      return m_reason.c_str();
    }

    bool HasExtension(const char *name)
    {
      string allExtensions(reinterpret_cast<char const * >(glGetString(GL_EXTENSIONS)));
      return allExtensions.find(name) != string::npos;
    }

    void DumpGLInformation()
    {
      LOG(LINFO, ("OpenGL Information"));
      LOG(LINFO, ("--------------------------------------------"));
      LOG(LINFO, ("Vendor     : ", glGetString(GL_VENDOR)));
      LOG(LINFO, ("Renderer   : ", glGetString(GL_RENDERER)));
      LOG(LINFO, ("Version    : ", glGetString(GL_VERSION)));

      vector<string> names;

      strings::Tokenize(string(reinterpret_cast<char const *>(glGetString(GL_EXTENSIONS))), " ", MakeBackInsertFunctor(names));

      for (unsigned i = 0; i < names.size(); ++i)
      {
        if (i == 0)
          LOG(LINFO, ("Extensions : ", names[i]));
        else
          LOG(LINFO, ("             ", names[i]));
      }

      LOG(LINFO, ("--------------------------------------------"));
    }

    // basic opengl functions and constants
    void (OPENGL_CALLING_CONVENTION * glEnableFn)(GLenum cap);
    void (OPENGL_CALLING_CONVENTION * glDisableFn)(GLenum cap);
    void (OPENGL_CALLING_CONVENTION * glAlphaFuncFn)(GLenum func, GLclampf ref);

    void (OPENGL_CALLING_CONVENTION * glEnableClientStateFn) (GLenum array);
    void (OPENGL_CALLING_CONVENTION * glVertexPointerFn) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
    void (OPENGL_CALLING_CONVENTION * glNormalPointerFn) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
    void (OPENGL_CALLING_CONVENTION * glTexCoordPointerFn) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);

    void (OPENGL_CALLING_CONVENTION * glMatrixModeFn) (GLenum mode);
    void (OPENGL_CALLING_CONVENTION * glLoadIdentityFn)();
    void (OPENGL_CALLING_CONVENTION * glLoadMatrixfFn) (const GLfloat *m);
    void (OPENGL_CALLING_CONVENTION * glOrthoFn) (GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar);
    void (OPENGL_CALLING_CONVENTION * glDrawElementsFn) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);

    bool g_isBufferObjectsSupported = true;
    bool g_isMapBufferSupported = true;
    bool g_isFramebufferSupported = true;
    bool g_isRenderbufferSupported = true;
    bool g_isSeparateBlendFuncSupported = false;

    void (OPENGL_CALLING_CONVENTION * glBindBufferFn) (GLenum target, GLuint buffer);
    void (OPENGL_CALLING_CONVENTION * glGenBuffersFn) (GLsizei n, GLuint *buffers);
    void (OPENGL_CALLING_CONVENTION * glBufferDataFn) (GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
    void (OPENGL_CALLING_CONVENTION * glBufferSubDataFn) (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data);
    void (OPENGL_CALLING_CONVENTION * glDeleteBuffersFn) (GLsizei n, const GLuint *buffers);
    void * (OPENGL_CALLING_CONVENTION * glMapBufferFn) (GLenum target, GLenum access);
    GLboolean (OPENGL_CALLING_CONVENTION * glUnmapBufferFn) (GLenum target);

    void (OPENGL_CALLING_CONVENTION * glBindFramebufferFn) (GLenum target, GLuint framebuffer);
    void (OPENGL_CALLING_CONVENTION * glFramebufferTexture2DFn) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
    void (OPENGL_CALLING_CONVENTION * glFramebufferRenderbufferFn) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
    void (OPENGL_CALLING_CONVENTION * glGenFramebuffersFn) (GLsizei n, GLuint *framebuffers);
    void (OPENGL_CALLING_CONVENTION * glDeleteFramebuffersFn) (GLsizei n, const GLuint *framebuffers);
    GLenum (OPENGL_CALLING_CONVENTION * glCheckFramebufferStatusFn) (GLenum target);

    void (OPENGL_CALLING_CONVENTION * glGenRenderbuffersFn) (GLsizei n, GLuint *renderbuffers);
    void (OPENGL_CALLING_CONVENTION * glDeleteRenderbuffersFn) (GLsizei n, const GLuint *renderbuffers);
    void (OPENGL_CALLING_CONVENTION * glBindRenderbufferFn) (GLenum target, GLuint renderbuffer);
    void (OPENGL_CALLING_CONVENTION * glRenderbufferStorageFn) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);

    void (OPENGL_CALLING_CONVENTION * glBlendFuncSeparateFn) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);

    bool g_hasContext = true;
    bool g_doLogOGLCalls = false;

    void CheckExtensionSupport()
    {
      if (!g_isFramebufferSupported)
        throw platform_unsupported("no framebuffer support");

      if (!g_isRenderbufferSupported)
        throw platform_unsupported("no renderbuffer support");
    }

    void LogError(char const * err, my::SrcPoint const & srcPt)
    {
      if (err)
      {
#ifdef OMIM_OS_BADA
        AppLog("%s", err);
#endif
        LOG(LERROR, (err, srcPt.FileName(), srcPt.Line()));
      }
    }

    char const * Error2String(int error)
    {
      switch (error)
      {
      case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
      case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
      case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
//      case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW"; //< not supported in OpenGL ES2.0
//      case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW"; //< not supported in OpenGL ES2.0
      case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
#ifdef OMIM_OS_BADA        /* Errors / GetError return values */
      case EGL_SUCCESS : return 0;
      case EGL_NOT_INITIALIZED : return "EGL_NOT_INITIALIZED";
      case EGL_BAD_ACCESS : return "EGL_BAD_ACCESS";
      case EGL_BAD_ALLOC : return "EGL_BAD_ALLOC";
      case EGL_BAD_ATTRIBUTE : return "EGL_BAD_ATTRIBUTE";
      case EGL_BAD_CONFIG : return "EGL_BAD_CONFIG";
      case EGL_BAD_CONTEXT : return "EGL_BAD_CONTEXT";
      case EGL_BAD_CURRENT_SURFACE : return "EGL_BAD_CURRENT_SURFACE";
      case EGL_BAD_DISPLAY : return "EGL_BAD_DISPLAY";
      case EGL_BAD_MATCH : return "EGL_BAD_MATCH";
      case EGL_BAD_NATIVE_PIXMAP : return "EGL_BAD_NATIVE_PIXMAP";
      case EGL_BAD_NATIVE_WINDOW : return "EGL_BAD_NATIVE_WINDOW";
      case EGL_BAD_PARAMETER : return "EGL_BAD_PARAMETER";
      case EGL_BAD_SURFACE : return "EGL_BAD_SURFACE";
      case EGL_CONTEXT_LOST : return "EGL_CONTEXT_LOST";
#endif
      default: return 0;
      }
    }

    void CheckError(my::SrcPoint const & srcPt)
    {
      LogError(Error2String(glGetError()), srcPt);
    }

#ifdef OMIM_OS_BADA
    void CheckEGLError(my::SrcPoint const & srcPt)
    {
      LogError(Error2String(eglGetError()), srcPt);
    }
#endif
  }
}
