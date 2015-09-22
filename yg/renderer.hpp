#pragma once

#include "color.hpp"
#include "packets_queue.hpp"
#include "resource_manager.hpp"

#include "../base/threaded_list.hpp"
#include "../base/commands_queue.hpp"
#include "../std/function.hpp"
#include "../std/shared_ptr.hpp"
#include "../geometry/rect2d.hpp"

namespace yg
{
  class ResourceManager;

  namespace gl
  {
    class FrameBuffer;
    class RenderBuffer;
    class BaseTexture;
    class RenderTarget;

    class Renderer
    {
    public:

      struct ClearCommand : Command
      {
        yg::Color m_color;
        bool m_clearRT;
        float m_depth;
        bool m_clearDepth;

        ClearCommand(yg::Color const & color,
                     bool clearRT = true,
                     float depth = 1.0,
                     bool clearDepth = true);

        void perform();
      };

      struct ReadPixels : Command
      {
        m2::RectU m_rect;
        void * m_data;

        ReadPixels(m2::RectU const & r, void * data);

        void perform();
      };

      struct ChangeFrameBuffer : Command
      {
        shared_ptr<FrameBuffer> m_frameBuffer;

        ChangeFrameBuffer(shared_ptr<FrameBuffer> const & fb);

        void perform();
      };

      struct UnbindRenderTarget : Command
      {
        shared_ptr<RenderTarget> m_renderTarget;
        UnbindRenderTarget(shared_ptr<RenderTarget> const & renderTarget);
        void perform();
      };

      struct DiscardFramebuffer : Command
      {
        bool m_doDiscardColor;
        bool m_doDiscardDepth;
        DiscardFramebuffer(bool doDiscardColor, bool doDiscardDepth);
        void perform();
      };

      struct FinishCommand : Command
      {
        void perform();
      };

      virtual ~Renderer();

      struct Params
      {
        shared_ptr<ResourceManager> m_resourceManager;
        shared_ptr<FrameBuffer> m_frameBuffer;
        bool m_isDebugging;
        bool m_doUnbindRT;
        bool m_isSynchronized;
        PacketsQueue * m_renderQueue;
        Params();
      };

    private:

      shared_ptr<FrameBuffer> m_frameBuffer;
      shared_ptr<RenderTarget> m_renderTarget;
      shared_ptr<RenderBuffer> m_depthBuffer;
      shared_ptr<ResourceManager> m_resourceManager;

      PacketsQueue * m_renderQueue;

      bool m_isDebugging;

      bool m_doUnbindRT;
      bool m_isSynchronized;

      bool m_isRendering;

      unsigned int m_width;
      unsigned int m_height;

      core::CommandsQueue::Environment const * m_env;

    public:

      static const yg::Color s_bgColor;

      Renderer(Params const & params = Params());

      void beginFrame();
      void endFrame();

      bool isRendering() const;

      shared_ptr<ResourceManager> const & resourceManager() const;

      shared_ptr<FrameBuffer> const & frameBuffer() const;

      void setRenderTarget(shared_ptr<RenderTarget> const & rt);
      shared_ptr<RenderTarget> const & renderTarget() const;
      void resetRenderTarget();

      void unbindRenderTarget();

      void setDepthBuffer(shared_ptr<RenderBuffer> const & rt);
      shared_ptr<RenderBuffer> const & depthBuffer() const;
      void resetDepthBuffer();

      void discardFramebuffer(bool doDiscardColor, bool doDiscardDepth);

      /// @param clearRT - should we clear the renderTarget data (visible pixels)?
      /// @param clearDepth - should we clear depthBuffer data?
      /// @warning this function respects the clipping rect set and enabled(!)
      ///          by the setClipRect/enableClipRect. Whether the clipping is
      ///          not enabled -  the entire currently bound render surface is used.
      void clear(yg::Color const & c, bool clearRT = true, float depth = 1.0, bool clearDepth = true);

      void onSize(unsigned width, unsigned height);

      unsigned int width() const;
      unsigned int height() const;

      void finish(bool doForce = false);
      void readPixels(m2::RectU const & r, void * data, bool doForce = false);

      bool isDebugging() const;

      void processCommand(shared_ptr<Command> const & command, Packet::EType type = Packet::ECommand, bool doForce = false);
      PacketsQueue * renderQueue();

      void addCheckPoint();
      void addFramePoint();

      void completeCommands();

      void setEnvironment(core::CommandsQueue::Environment const * env);
      bool isCancelled() const;

      void setPixelPrecision(bool flag);
    };
  }
}
