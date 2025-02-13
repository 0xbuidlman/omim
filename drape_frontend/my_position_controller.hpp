#pragma once

#include "drape_frontend/my_position.hpp"

#include "drape/gpu_program_manager.hpp"
#include "drape/uniform_values_storage.hpp"

#include "platform/location.hpp"

#include "geometry/screenbase.hpp"

#include "base/timer.hpp"

#include "std/function.hpp"

namespace df
{

class Animation;
using TAnimationCreator = function<drape_ptr<Animation>(double)>;

class MyPositionController
{
public:
  class Listener
  {
  public:
    virtual ~Listener() {}
    virtual void PositionChanged(m2::PointD const & position) = 0;
    /// Show map with center in "center" point and current zoom
    virtual void ChangeModelView(m2::PointD const & center, int zoomLevel, TAnimationCreator const & parallelAnimCreator) = 0;
    /// Change azimuth of current ModelView
    virtual void ChangeModelView(double azimuth, TAnimationCreator const & parallelAnimCreator) = 0;
    /// Somehow show map that "rect" will see
    virtual void ChangeModelView(m2::RectD const & rect, TAnimationCreator const & parallelAnimCreator) = 0;
    /// Show map where "usePos" (mercator) placed in "pxZero" on screen and map rotated around "userPos"
    virtual void ChangeModelView(m2::PointD const & userPos, double azimuth, m2::PointD const & pxZero,
                                 int zoomLevel, TAnimationCreator const & parallelAnimCreator) = 0;
    virtual void ChangeModelView(double autoScale, m2::PointD const & userPos, double azimuth, m2::PointD const & pxZero,
                                 TAnimationCreator const & parallelAnimCreator) = 0;
  };

  MyPositionController(location::EMyPositionMode initMode, double timeInBackground,
                       bool isFirstLaunch, bool isRoutingActive, bool isAutozoomEnabled);
  ~MyPositionController();

  void OnNewViewportRect();
  void UpdatePixelPosition(ScreenBase const & screen);
  void SetListener(ref_ptr<Listener> listener);

  m2::PointD const & Position() const;
  double GetErrorRadius() const;

  bool IsModeHasPosition() const;

  void DragStarted();
  void DragEnded(m2::PointD const & distance);

  void ScaleStarted();
  void ScaleEnded();

  void Rotated();

  void ResetRoutingNotFollowTimer();
  void ResetBlockAutoZoomTimer();

  void CorrectScalePoint(m2::PointD & pt) const;
  void CorrectScalePoint(m2::PointD & pt1, m2::PointD & pt2) const;
  void CorrectGlobalScalePoint(m2::PointD & pt) const;

  void SetRenderShape(drape_ptr<MyPosition> && shape);
  void ResetRenderShape();

  void ActivateRouting(int zoomLevel, bool enableAutoZoom);
  void DeactivateRouting();

  void EnablePerspectiveInRouting(bool enablePerspective);
  void EnableAutoZoomInRouting(bool enableAutoZoom);

  void StopLocationFollow();
  void NextMode(ScreenBase const & screen);
  void LoseLocation();

  void SetTimeInBackground(double time);

  void OnCompassTapped();

  void OnLocationUpdate(location::GpsInfo const & info, bool isNavigable, ScreenBase const & screen);
  void OnCompassUpdate(location::CompassInfo const & info, ScreenBase const & screen);

  void SetModeListener(location::TMyPositionModeChanged const & fn);

  void Render(ScreenBase const & screen, int zoomLevel, ref_ptr<dp::GpuProgramManager> mng,
              dp::UniformValuesStorage const & commonUniforms);

  bool IsRotationAvailable() const { return m_isDirectionAssigned; }
  bool IsInRouting() const { return m_isInRouting; }
  bool IsRouteFollowingActive() const;
  bool IsWaitingForTimers() const;

  bool IsWaitingForLocation() const;
  m2::PointD GetDrawablePosition();

private:
  bool IsModeChangeViewport() const;
  void ChangeMode(location::EMyPositionMode newMode);
  void SetDirection(double bearing);
  
  bool IsInStateWithPosition() const;

  bool IsVisible() const { return m_isVisible; }
  void SetIsVisible(bool isVisible) { m_isVisible = isVisible; }

  void ChangeModelView(m2::PointD const & center, int zoomLevel);
  void ChangeModelView(double azimuth);
  void ChangeModelView(m2::RectD const & rect);
  void ChangeModelView(m2::PointD const & userPos, double azimuth, m2::PointD const & pxZero, int zoomLevel);
  void ChangeModelView(double autoScale, m2::PointD const & userPos, double azimuth, m2::PointD const & pxZero);

  void UpdateViewport(int zoomLevel);
  bool UpdateViewportWithAutoZoom();
  m2::PointD GetRotationPixelCenter() const;
  m2::PointD GetRoutingRotationPixelCenter() const;

  double GetDrawableAzimut();
  void CreateAnim(m2::PointD const & oldPos, double oldAzimut, ScreenBase const & screen);

  bool AlmostCurrentPosition(m2::PointD const & pos) const;
  bool AlmostCurrentAzimut(double azimut) const;

private:
  location::EMyPositionMode m_mode;
  location::EMyPositionMode m_desiredInitMode;
  bool m_isFirstLaunch;

  bool m_isInRouting;

  bool m_needBlockAnimation;
  bool m_wasRotationInScaling;

  location::TMyPositionModeChanged m_modeChangeCallback;
  drape_ptr<MyPosition> m_shape;
  ref_ptr<Listener> m_listener;

  double m_errorRadius;  // error radius in mercator
  m2::PointD m_position; // position in mercator
  double m_drawDirection;
  m2::PointD m_oldPosition; // position in mercator
  double m_oldDrawDirection;

  bool m_enablePerspectiveInRouting;
  bool m_enableAutoZoomInRouting;
  double m_autoScale2d;
  double m_autoScale3d;

  my::Timer m_lastGPSBearing;
  my::Timer m_pendingTimer;
  my::Timer m_routingNotFollowTimer;
  my::Timer m_blockAutoZoomTimer;
  my::Timer m_updateLocationTimer;
  double m_lastLocationTimestamp;

  m2::RectD m_pixelRect;
  double m_positionYOffset;

  bool m_isVisible;
  bool m_isDirtyViewport;
  bool m_isDirtyAutoZoom;
  bool m_isPendingAnimation;

  TAnimationCreator m_animCreator;

  bool m_isPositionAssigned;
  bool m_isDirectionAssigned;

  bool m_positionIsObsolete;
  bool m_needBlockAutoZoom;

  bool m_notFollowAfterPending;
};

}
