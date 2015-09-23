#include "../base/SRC_FIRST.hpp"

#include "render_policy.hpp"

#include "window_handle.hpp"
#include "tiling_render_policy_st.hpp"
#include "tiling_render_policy_mt.hpp"
#include "proto_to_styles.hpp"
#include "scales_processor.hpp"

#include "../anim/controller.hpp"
#include "../anim/task.hpp"

#include "../graphics/opengl/opengl.hpp"
#include "../graphics/opengl/gl_render_context.hpp"
#include "../graphics/opengl/renderbuffer.hpp"

#include "../indexer/scales.hpp"
#include "../indexer/drawing_rules.hpp"

#include "../platform/video_timer.hpp"
#include "../platform/settings.hpp"
#include "../platform/platform.hpp"

RenderPolicy::~RenderPolicy()
{
  LOG(LDEBUG, ("clearing cached drawing rules"));
  drule::rules().ClearCaches();
  if (m_primaryRC && m_resourceManager)
    m_primaryRC->endThreadDrawing(m_resourceManager->guiThreadSlot());
}

RenderPolicy::RenderPolicy(Params const & p,
                           size_t idCacheSize)
  : m_bgColor(ConvertColor(drule::rules().GetBgColor())),
    m_primaryRC(p.m_primaryRC),
    m_doForceUpdate(false),
    m_density(p.m_density),
    m_visualScale(graphics::visualScale(p.m_density)),
    m_skinName(p.m_skinName),
    m_useOffscreenRendering(p.m_useOffscreenRendering),
    m_offscreenWidth(p.m_offscreenWidth),
    m_offscreenHeight(p.m_offscreenHeight)
{
  LOG(LDEBUG, ("each BaseRule will hold up to", idCacheSize, "cached values"));
  drule::rules().ResizeCaches(idCacheSize);

  graphics::gl::InitExtensions();
  graphics::gl::CheckExtensionSupport();
}

void RenderPolicy::InitCacheScreen()
{
  graphics::Screen::Params cp;

  cp.m_threadSlot = m_resourceManager->guiThreadSlot();
  cp.m_storageType = graphics::ETinyStorage;
  cp.m_textureType = graphics::ESmallTexture;
  cp.m_resourceManager = m_resourceManager;
  cp.m_renderContext = m_primaryRC;

  m_cacheScreen.reset(CreateScreenWithParams(cp));
}

graphics::Screen * RenderPolicy::CreateScreenWithParams(graphics::Screen::Params const & params) const
{
 return new graphics::Screen(params);
}

void RenderPolicy::OnSize(int w, int h)
{
  if (m_cacheScreen)
    m_cacheScreen->onSize(w, h);
  m_drawer->onSize(w, h);
}

void RenderPolicy::StartDrag()
{
  m_windowHandle->invalidate();
}

void RenderPolicy::DoDrag()
{
  m_windowHandle->invalidate();
}

void RenderPolicy::StopDrag()
{
  m_windowHandle->invalidate();
}

void RenderPolicy::StartScale()
{
  m_windowHandle->invalidate();
}

void RenderPolicy::DoScale()
{
  m_windowHandle->invalidate();
}

void RenderPolicy::StopScale()
{
  m_windowHandle->invalidate();
}

void RenderPolicy::StartRotate(double a, double)
{
  m_windowHandle->invalidate();
}

void RenderPolicy::DoRotate(double a, double)
{
  m_windowHandle->invalidate();
}

void RenderPolicy::StopRotate(double a, double)
{
  m_windowHandle->invalidate();
}

void RenderPolicy::BeginFrame(shared_ptr<PaintEvent> const & e, ScreenBase const & s)
{
  /// processing animations at the beginning of the frame.
  /// it's crucial as in this function could happen transition from
  /// animating to non-animating state which should be properly handled
  /// in the following RenderPolicy::DrawFrame call.
  m_controller->PerformStep();
}

void RenderPolicy::EndFrame(shared_ptr<PaintEvent> const & e, ScreenBase const & s)
{
}

void RenderPolicy::DrawFullFrame(shared_ptr<PaintEvent> const & e, ScreenBase const & s)
{
  m_resourceManager->updatePoolState();

  Drawer * const drawer = e->drawer();
  if (nullptr == drawer)
    return;

  graphics::Screen * const screen = drawer->screen();
  if (nullptr == screen)
    return;

  uint const w = s.GetWidth();
  uint const h = s.GetHeight();

  m2::RectI const renderRectI(0, 0, w, h);
  m2::RectD const renderRectD(renderRectI);

  shared_ptr<graphics::OverlayStorage> overlay(new graphics::OverlayStorage(renderRectD));

  screen->setOverlay(overlay);
  screen->beginFrame();
  screen->setClipRect(renderRectI);
  screen->clear(m_bgColor);

  ASSERT((m_renderFn != nullptr), ());
  m_renderFn(e, s, renderRectD, ScalesProcessor(TileSize()).GetTileScaleBase(s));

  screen->applySharpStates();
  screen->resetOverlay();
  screen->clear(m_bgColor, false);

  graphics::Overlay drawOverlay;
  drawOverlay.merge(overlay);

  overlay.reset();

  math::Matrix<double, 3, 3> const m = math::Identity<double, 3>();
  drawOverlay.forEach([&screen, &m](shared_ptr<graphics::OverlayElement> const & element)
  {
    element->draw(screen, m);
  });

  screen->endFrame();
}

bool RenderPolicy::NeedRedraw() const
{
  return m_windowHandle->needRedraw()
      || IsAnimating();
}

bool RenderPolicy::IsAnimating() const
{
  return (m_controller->HasVisualTasks()
      || (m_controller->LockCount() > 0));
}

bool RenderPolicy::IsTiling() const
{
  return false;
}

shared_ptr<Drawer> const & RenderPolicy::GetDrawer() const
{
  return m_drawer;
}

shared_ptr<Drawer> const & RenderPolicy::GetOffscreenDrawer() const
{
  return m_offscreenDrawer;
}

shared_ptr<WindowHandle> const & RenderPolicy::GetWindowHandle() const
{
  return m_windowHandle;
}

graphics::GlyphCache * RenderPolicy::GetGlyphCache() const
{
  return m_resourceManager->glyphCache(m_resourceManager->guiThreadSlot());
}

void RenderPolicy::SetRenderFn(TRenderFn const & renderFn)
{
  m_renderFn = renderFn;
}

bool RenderPolicy::DoForceUpdate() const
{
  return m_doForceUpdate;
}

void RenderPolicy::SetForceUpdate(bool flag)
{
  m_doForceUpdate = flag;
}

void RenderPolicy::SetInvalidRect(m2::AnyRectD const & glbRect)
{
  m_invalidRect = glbRect;
}

m2::AnyRectD const & RenderPolicy::GetInvalidRect() const
{
  return m_invalidRect;
}

bool RenderPolicy::IsEmptyModel() const
{
  return false;
}

double RenderPolicy::VisualScale() const
{
  return m_visualScale;
}

graphics::EDensity RenderPolicy::Density() const
{
  return m_density;
}

string const & RenderPolicy::SkinName() const
{
  return m_skinName;
}

int RenderPolicy::InsertBenchmarkFence()
{
  return 0;
}

void RenderPolicy::JoinBenchmarkFence(int fenceID)
{
}

void RenderPolicy::SetAnimController(anim::Controller * controller)
{
  m_controller = controller;
}

void RenderPolicy::FrameLock()
{
  LOG(LDEBUG/*LWARNING*/, ("unimplemented method called."));
}

void RenderPolicy::FrameUnlock()
{
  LOG(LDEBUG/*LWARNING*/, ("unimplemented method called"));
}

graphics::Overlay * RenderPolicy::FrameOverlay() const
{
  LOG(LDEBUG/*LWARNING*/, ("unimplemented method called"));
  return NULL;
}

graphics::Color const RenderPolicy::GetBgColor() const
{
  return m_bgColor;
}

shared_ptr<graphics::Screen> const & RenderPolicy::GetCacheScreen() const
{
  return m_cacheScreen;
}

void RenderPolicy::InitWindowsHandle(VideoTimer * timer, shared_ptr<graphics::RenderContext> context)
{
  m_windowHandle.reset(new WindowHandle());
  m_windowHandle->setRenderPolicy(this);
  m_windowHandle->setUpdatesEnabled(false);
  m_windowHandle->setVideoTimer(timer);
  m_windowHandle->setRenderContext(context);
}

Drawer * RenderPolicy::CreateDrawer(bool isDefaultFB,
                                    shared_ptr<graphics::RenderContext> context,
                                    graphics::EStorageType storageType,
                                    graphics::ETextureType textureType)
{
  Drawer::Params dp;

  dp.m_frameBuffer = make_shared<graphics::gl::FrameBuffer>(isDefaultFB);
  dp.m_resourceManager = m_resourceManager;
  dp.m_threadSlot = m_resourceManager->guiThreadSlot();
  dp.m_visualScale = VisualScale();
  dp.m_storageType = storageType;
  dp.m_textureType = textureType;
  dp.m_renderContext = context;

  return new Drawer(dp);
}

Drawer * RenderPolicy::CreateOffscreenDrawer(shared_ptr<graphics::RenderContext> context,
                                             graphics::EStorageType storageType,
                                             graphics::ETextureType textureType)
{
  Drawer::Params dp;

  dp.m_frameBuffer = make_shared<graphics::gl::FrameBuffer>(false);
  dp.m_frameBuffer->onSize(m_offscreenWidth, m_offscreenHeight);
  dp.m_frameBuffer->setRenderTarget(make_shared<graphics::gl::RenderBuffer>(m_offscreenWidth, m_offscreenHeight, false));
  dp.m_frameBuffer->setDepthBuffer(make_shared<graphics::gl::RenderBuffer>(m_offscreenWidth, m_offscreenHeight, true));

  dp.m_resourceManager = m_resourceManager;
  dp.m_threadSlot = m_resourceManager->guiThreadSlot();
  dp.m_visualScale = VisualScale();
  dp.m_storageType = storageType;
  dp.m_textureType = textureType;
  dp.m_renderContext = context;

  return new Drawer(dp);
}

size_t RenderPolicy::GetLargeTextureSize(bool useNpot) const
{
  UNUSED_VALUE(useNpot);
  return 512;
}

size_t RenderPolicy::GetMediumTextureSize(bool useNpot) const
{
  uint32_t const size = 256 * VisualScale();
  if (useNpot)
    return size;

  return my::NextPowOf2(size);
}

size_t RenderPolicy::GetSmallTextureSize(bool useNpot) const
{
  uint32_t const size = 128 * VisualScale();
  if (useNpot)
    return size;

  return my::NextPowOf2(size);
}

graphics::ResourceManager::StoragePoolParams RenderPolicy::GetStorageParam(size_t vertexCount,
                                                                           size_t indexCount,
                                                                           size_t batchSize,
                                                                           graphics::EStorageType type)
{
  return graphics::ResourceManager::StoragePoolParams(vertexCount * sizeof(graphics::gl::Vertex),
                                                      sizeof(graphics::gl::Vertex),
                                                      indexCount * sizeof(unsigned short),
                                                      sizeof(unsigned short),
                                                      batchSize, type, false);
}

graphics::ResourceManager::TexturePoolParams RenderPolicy::GetTextureParam(size_t size,
                                                                           size_t initCount,
                                                                           graphics::DataFormat format,
                                                                           graphics::ETextureType type)
{
  LOG(LDEBUG, ("Texture creating size = ", size));
  return graphics::ResourceManager::TexturePoolParams(size, size, initCount, format, type, false);
}

RenderPolicy * CreateRenderPolicy(RenderPolicy::Params const & params)
{
  // @TODO!!! Check which policy is better for TIZEN
#if defined(OMIM_OS_ANDROID) || defined(OMIM_OS_TIZEN)
  return new TilingRenderPolicyST(params);
#endif
#ifdef OMIM_OS_IPHONE
  return new TilingRenderPolicyMT(params);
#endif
#ifdef OMIM_OS_DESKTOP
  return new TilingRenderPolicyST(params);
#endif
}
