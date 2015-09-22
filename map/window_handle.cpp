#include "window_handle.hpp"

WindowHandle::WindowHandle() :
  m_hasPendingUpdates(false),
  m_isUpdatesEnabled(true),
  m_needRedraw(true),
  m_stallsCount(0)
{
}

void WindowHandle::setVideoTimer(VideoTimer * videoTimer)
{
  m_videoTimer = videoTimer;
  m_frameFn = videoTimer->frameFn();
  m_videoTimer->setFrameFn(bind(&WindowHandle::checkedFrameFn, this));
  m_stallsCount = 0;
}

void WindowHandle::checkedFrameFn()
{
  if (needRedraw())
    m_stallsCount = 0;
  else
    ++m_stallsCount;

  if (m_stallsCount >= 60)
  {
//    LOG(LINFO, ("PausedDOWN"));
    m_videoTimer->pause();
  }
  else
    m_frameFn();
}

WindowHandle::~WindowHandle()
{
  m_videoTimer->stop();
}

bool WindowHandle::needRedraw() const
{
  return m_isUpdatesEnabled && m_needRedraw;
}

void WindowHandle::checkTimer()
{
  switch (m_videoTimer->state())
  {
  case VideoTimer::EStopped:
    m_videoTimer->start();
    break;
  case VideoTimer::EPaused:
//    LOG(LINFO, ("WokenUP"));
    m_videoTimer->resume();
    break;
  default:
    break;
  }
}

void WindowHandle::setNeedRedraw(bool flag)
{
  m_needRedraw = flag;
  if (m_needRedraw && m_isUpdatesEnabled)
    checkTimer();
}

shared_ptr<yg::gl::RenderContext> const & WindowHandle::renderContext()
{
  return m_renderContext;
}

void WindowHandle::setRenderContext(shared_ptr<yg::gl::RenderContext> const & renderContext)
{
  m_renderContext = renderContext;
}

bool WindowHandle::setUpdatesEnabled(bool doEnable)
{
  bool res = false;

  bool wasUpdatesEnabled = m_isUpdatesEnabled;
  m_isUpdatesEnabled = doEnable;

  if ((!wasUpdatesEnabled) && (doEnable) && (m_hasPendingUpdates))
  {
    setNeedRedraw(true);
    m_hasPendingUpdates = false;
    res = true;
  }

  return res;
}

void WindowHandle::invalidate()
{
  if (m_isUpdatesEnabled)
    setNeedRedraw(true);
  else
    m_hasPendingUpdates = true;
}
