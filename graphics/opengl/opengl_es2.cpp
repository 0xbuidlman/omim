#include "opengl.hpp"
#include "opengl_glsl_impl.hpp"

#ifdef OMIM_OS_ANDROID
  #define GL_GLEXT_PROTOTYPES
  #include <GLES2/gl2ext.h>
#endif

#ifdef OMIM_OS_IPHONE
  #include <TargetConditionals.h>
  #include <OpenGLES/ES2/glext.h>
#endif

namespace graphics
{
  namespace gl
  {
    const int GL_FRAMEBUFFER_BINDING_MWM = GL_FRAMEBUFFER_BINDING;
    const int GL_FRAMEBUFFER_MWM = GL_FRAMEBUFFER;
    const int GL_FRAMEBUFFER_UNSUPPORTED_MWM = GL_FRAMEBUFFER_UNSUPPORTED;
    const int GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_MWM = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
    const int GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_MWM = GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    const int GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_MWM = GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
    const int GL_FRAMEBUFFER_COMPLETE_MWM = GL_FRAMEBUFFER_COMPLETE;

    const int GL_DEPTH_ATTACHMENT_MWM = GL_DEPTH_ATTACHMENT;
    const int GL_COLOR_ATTACHMENT0_MWM = GL_COLOR_ATTACHMENT0;
    const int GL_RENDERBUFFER_MWM = GL_RENDERBUFFER;
    const int GL_RENDERBUFFER_BINDING_MWM = GL_RENDERBUFFER_BINDING;
    const int GL_DEPTH_COMPONENT16_MWM = GL_DEPTH_COMPONENT16;
    const int GL_DEPTH_COMPONENT24_MWM = GL_DEPTH_COMPONENT24_OES;
    const int GL_RGBA8_MWM = GL_RGBA8_OES;

    const int GL_WRITE_ONLY_MWM = GL_WRITE_ONLY_OES;

    void InitExtensions()
    {
      DumpGLInformation();

      g_isBufferObjectsSupported = true;

      glBindBufferFn = &glBindBuffer;
      glGenBuffersFn = &glGenBuffers;
      glBufferDataFn = &glBufferData;
      glBufferSubDataFn = &glBufferSubData;
      glDeleteBuffersFn = &glDeleteBuffers;

      g_isMapBufferSupported = HasExtension("GL_OES_mapbuffer");

      glMapBufferFn = &glMapBufferOES;
      glUnmapBufferFn = &glUnmapBufferOES;

      g_isFramebufferSupported = true;

      glBindFramebufferFn = &glBindFramebuffer;
      glFramebufferTexture2DFn = &glFramebufferTexture2D;
      glFramebufferRenderbufferFn = &glFramebufferRenderbuffer;
      glGenFramebuffersFn = &glGenFramebuffers;
      glDeleteFramebuffersFn = &glDeleteFramebuffers;
      glCheckFramebufferStatusFn = &glCheckFramebufferStatus;
      // this extension is defined in headers but absent in library on Android ARM platform
#if defined(OMIM_OS_ANDROID) && defined(__arm__)
      glDiscardFramebufferFn = 0;
#else
      glDiscardFramebufferFn = &glDiscardFramebufferEXT;
#endif

      g_isRenderbufferSupported = g_isFramebufferSupported;

      glGenRenderbuffersFn = &glGenRenderbuffers;
      glDeleteRenderbuffersFn = &glDeleteRenderbuffers;
      glBindRenderbufferFn = &glBindRenderbuffer;
      glRenderbufferStorageFn = &glRenderbufferStorage;

      g_isSeparateBlendFuncSupported = true;
      glBlendFuncSeparateFn = &glBlendFuncSeparate;

      glActiveTextureFn = &glActiveTexture;
      glGetAttribLocationFn = &glGetAttribLocation;
      glGetActiveAttribFn = &glGetActiveAttrib;
      glGetUniformLocationFn = &glGetUniformLocation;
      glGetActiveUniformFn = &glGetActiveUniform;
      glGetProgramInfoLogFn = &glGetProgramInfoLog;
      glGetProgramivFn = &glGetProgramiv;
      glLinkProgramFn = &glLinkProgram;
      glAttachShaderFn = &glAttachShader;
      glCreateProgramFn = &glCreateProgram;
      glDeleteProgramFn = & glDeleteProgram;
      glVertexAttribPointerFn = &glVertexAttribPointer;
      glEnableVertexAttribArrayFn = &glEnableVertexAttribArray;
      glUniformMatrix4fvFn = &glUniformMatrix4fv;
      glUniformMatrix3fvFn = &glUniformMatrix3fv;
      glUniformMatrix2fvFn = &glUniformMatrix2fv;
      glUniform4iFn = &glUniform4i;
      glUniform3iFn = &glUniform3i;
      glUniform2iFn = &glUniform2i;
      glUniform1iFn = &glUniform1i;
      glUniform4fFn = &glUniform4f;
      glUniform3fFn = &glUniform3f;
      glUniform2fFn = &glUniform2f;
      glUniform1fFn = &glUniform1f;
      glUseProgramFn = &glUseProgram;
      glGetShaderInfoLogFn = &glGetShaderInfoLog;
      glGetShaderivFn = &glGetShaderiv;
      glCompileShaderFn = &glCompileShader;
      glShaderSourceFn = &glShaderSource;
      glCreateShaderFn = &glCreateShader;
      glDeleteShaderFn = &glDeleteShader;
    }
  }
}

