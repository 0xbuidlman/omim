#pragma once

#include "events.hpp"
#ifndef USE_DRAPE
  #include "render_policy.hpp"
  #include "window_handle.hpp"
#else
  #include "../drape/oglcontextfactory.hpp"
  #include "../drape_frontend/drape_engine.hpp"
#endif // USE_DRAPE
#include "feature_vec_model.hpp"
#include "information_display.hpp"
#include "location_state.hpp"
#include "navigator.hpp"
#include "extra_map_screen.h"
#include "animator.hpp"
#include "scales_processor.hpp"

#include "bookmark.hpp"
#include "bookmark_manager.hpp"
#include "pin_click_manager.hpp"

#include "mwm_url.hpp"
#include "move_screen_task.hpp"
#include "track.hpp"
#include "routing_session.hpp"
#include "country_tree.hpp"

#include "../search/search_engine.hpp"

#include "../storage/storage.hpp"

#include "../platform/location.hpp"

#ifndef USE_DRAPE
  #include "../graphics/defines.hpp"
  #include "../graphics/screen.hpp"
  #include "../graphics/color.hpp"
#endif // USE_DRAPE

#include "../geometry/rect2d.hpp"
#include "../geometry/screenbase.hpp"

#include "../indexer/map_style.hpp"

#include "../base/strings_bundle.hpp"

#include "../std/vector.hpp"
#include "../std/shared_ptr.hpp"
#include "../std/unique_ptr.hpp"
#include "../std/target_os.hpp"

namespace search
{
  class Result;
  class Results;
  struct AddressInfo;
}

namespace gui { class Controller; }
namespace anim { class Controller; }

class CountryStatusDisplay;
class BenchmarkEngine;

/// Uncomment line to make fixed position settings and
/// build version for screenshots.
//#define FIXED_LOCATION

class Framework
{
#ifdef FIXED_LOCATION
  class FixedPosition
  {
    pair<double, double> m_latlon;
    double m_dirFromNorth;
    bool m_fixedLatLon, m_fixedDir;

  public:
    FixedPosition();

    void GetLat(double & l) const { if (m_fixedLatLon) l = m_latlon.first; }
    void GetLon(double & l) const { if (m_fixedLatLon) l = m_latlon.second; }

    void GetNorth(double & n) const { if (m_fixedDir) n = m_dirFromNorth; }
    bool HasNorth() const { return m_fixedDir; }

  } m_fixedPos;
#endif

protected:
  friend class BenchmarkEngine;

  StringsBundle m_stringsBundle;

  mutable unique_ptr<search::Engine> m_pSearchEngine;

  model::FeaturesFetcher m_model;
  ScalesProcessor m_scales;
  Navigator m_navigator;
  Animator m_animator;
  ExtraMapScreen m_extraMapScreen;

  routing::RoutingSession m_routingSession;

  typedef vector<BookmarkCategory *>::iterator CategoryIter;

#ifndef USE_DRAPE
  unique_ptr<RenderPolicy> m_renderPolicy;
#else
  dp::MasterPointer<df::DrapeEngine> m_drapeEngine;
#endif

  double m_StartForegroundTime;

  bool m_queryMaxScaleMode;

  int m_width;
  int m_height;

  void StopLocationFollow();

  storage::Storage m_storage;
  storage::CountryTree m_countryTree;
  unique_ptr<gui::Controller> m_guiController;
  unique_ptr<anim::Controller> m_animController;
  InformationDisplay m_informationDisplay;

  /// How many pixels around touch point are used to get bookmark or POI
  static const int TOUCH_PIXEL_RADIUS = 20;

  /// This function is called by m_storage to notify that country downloading is finished.
  /// @param[in] file Country file name (without extensions).
  void UpdateAfterDownload(string const & file, storage::TMapOptions opt);

  //my::Timer m_timer;
  inline double ElapsedSeconds() const
  {
    //return m_timer.ElapsedSeconds();
    return 0.0;
  }
#ifndef USE_DRAPE
  void DrawAdditionalInfo(shared_ptr<PaintEvent> const & e);
#endif // USE_DRAPE

  BenchmarkEngine * m_benchmarkEngine;

  BookmarkManager m_bmManager;
  PinClickManager m_balloonManager;

  void ClearAllCaches();

  void RemoveMap(string const & file);

  /// Deletes user calculated indexes on country updates
  void DeleteCountryIndexes(string const & mwmName);

public:
  Framework();
  virtual ~Framework();

  void SetRenderAsyncCallback(std::function<void(ExtraMapScreen::MapImage const &)> f)
  {
    m_extraMapScreen.SetRenderAsyncCallback(f);
  }
  void RenderAsync(m2::PointD const & center, size_t scale, size_t width, size_t height)
  {
    m_extraMapScreen.RenderAsync(center, scale, width, height);
  }

  /// @name Process storage connecting/disconnecting.
  //@{
  /// @param[out] maps  File names without path.
  void GetMaps(vector<string> & maps) const;

  void AddMaps();
  void RemoveMaps();

  /// @return Inner mwm data version from header or -1 in case of error.
  int AddMap(string const & file);
  //@}

  /// @name This functions is used by Downloader UI.
  //@{
  /// options - flags that signal about parts of map that must be deleted
  void DeleteCountry(storage::TIndex const & index, storage::TMapOptions opt);
  /// options - flags that signal about parts of map that must be downloaded
  void DownloadCountry(storage::TIndex const & index, storage::TMapOptions opt);

  storage::TStatus GetCountryStatus(storage::TIndex const & index) const;
  string GetCountryName(storage::TIndex const & index) const;

  /// Get country rect from borders (not from mwm file).
  /// @param[in] file Pass country file name without extension as an id.
  m2::RectD GetCountryBounds(string const & file) const;
  m2::RectD GetCountryBounds(storage::TIndex const & index) const;

  void ShowCountry(storage::TIndex const & index);
  //@}

  /// Scans and loads all kml files with bookmarks in WritableDir.
  void LoadBookmarks();

  /// @return Created bookmark index in category.
  size_t AddBookmark(size_t categoryIndex, m2::PointD const & ptOrg, BookmarkData & bm);
  /// @return New moved bookmark index in category.
  size_t MoveBookmark(size_t bmIndex, size_t curCatIndex, size_t newCatIndex);
  void ReplaceBookmark(size_t catIndex, size_t bmIndex, BookmarkData const & bm);
  /// @return Created bookmark category index.
  size_t AddCategory(string const & categoryName);

  inline size_t GetBmCategoriesCount() const { return m_bmManager.GetBmCategoriesCount(); }
  /// @returns 0 if category is not found
  BookmarkCategory * GetBmCategory(size_t index) const;

  size_t LastEditedBMCategory() { return m_bmManager.LastEditedBMCategory(); }
  string LastEditedBMType() const { return m_bmManager.LastEditedBMType(); }

  /// Delete bookmarks category with all bookmarks.
  /// @return true if category was deleted
  bool DeleteBmCategory(size_t index);

  void ShowBookmark(BookmarkAndCategory const & bnc);
  void ShowTrack(Track const & track);

  void ClearBookmarks();

  bool AddBookmarksFile(string const & filePath);

  inline m2::PointD PtoG(m2::PointD const & p) const { return m_navigator.PtoG(p); }
  inline m2::PointD GtoP(m2::PointD const & p) const { return m_navigator.GtoP(p); }

  storage::Storage & Storage() { return m_storage; }
  storage::CountryTree & GetCountryTree() { return m_countryTree; }

  /// @name GPS location updates routine.
  //@{
  void OnLocationError(location::TLocationError error);
  void OnLocationUpdate(location::GpsInfo const & info);
  void OnCompassUpdate(location::CompassInfo const & info);
  //@}

#ifndef USE_DRAPE
  void SetRenderPolicy(RenderPolicy * renderPolicy);
  void InitGuiSubsystem();
  RenderPolicy * GetRenderPolicy() const;
#else
  void CreateDrapeEngine(dp::RefPointer<dp::OGLContextFactory> contextFactory, float vs, int w, int h);
#endif // USE_DRAPE

  void SetMapStyle(MapStyle mapStyle);
  MapStyle GetMapStyle() const;

  InformationDisplay & GetInformationDisplay();
  CountryStatusDisplay * GetCountryStatusDisplay() const;

  /// Safe function to get current visual scale.
  /// Call it when you need do calculate pixel rect (not matter if m_renderPolicy == 0).
  /// @return 1.0 if m_renderPolicy == 0 (possible for Android).
  double GetVisualScale() const;

  void PrepareToShutdown();

  void SetupMeasurementSystem();

#ifndef USE_DRAPE
  RenderPolicy::TRenderFn DrawModelFn();

  void DrawModel(shared_ptr<PaintEvent> const & e,
                 ScreenBase const & screen,
                 m2::RectD const & renderRect,
                 int baseScale, bool isTilingQuery);
#endif // USE_DRAPE

private:
  search::Engine * GetSearchEngine() const;
  search::SearchParams m_lastSearch;
  uint8_t m_fixedSearchResults;

  void OnSearchResultsCallback(search::Results const & results);
  void OnSearchResultsCallbackUI(search::Results const & results);
  void FillSearchResultsMarks(search::Results const & results);

public:
  m2::RectD GetCurrentViewport() const;

  void UpdateUserViewportChanged();

  /// Call this function before entering search GUI.
  /// While it's loading, we can cache features near user's position.
  /// @param[in] hasPt Are (lat, lon) valid
  /// @param[in] (lat, lon) Current user's position
  void PrepareSearch(bool hasPt, double lat = 0.0, double lon = 0.0);
  bool Search(search::SearchParams const & params);
  bool GetCurrentPosition(double & lat, double & lon) const;

  void ShowSearchResult(search::Result const & res);
  size_t ShowAllSearchResults();

  void StartInteractiveSearch(search::SearchParams const & params) { m_lastSearch = params; }
  bool IsISActive() const { return !m_lastSearch.m_query.empty(); }
  void CancelInteractiveSearch();

  /// Calculate distance and direction to POI for the given position.
  /// @param[in]  point             POI's position;
  /// @param[in]  lat, lon, north   Current position and heading from north;
  /// @param[out] distance          Formatted distance string;
  /// @param[out] azimut            Azimut to point from (lat, lon);
  /// @return true  If the POI is near the current position (distance < 25 km);
  bool GetDistanceAndAzimut(m2::PointD const & point,
                            double lat, double lon, double north,
                            string & distance, double & azimut);

  /// @name Get any country info by point.
  //@{
  storage::TIndex GetCountryIndex(m2::PointD const & pt) const;

  string GetCountryName(m2::PointD const & pt) const;
  /// @param[in] id Country file name without an extension.
  string GetCountryName(string const & id) const;

  /// @return country code in ISO 3166-1 alpha-2 format (two small letters) or empty string
  string GetCountryCode(m2::PointD const & pt) const;
  //@}

  void SetMaxWorldRect();

  void Invalidate(bool doForceUpdate = false);
  void InvalidateRect(m2::RectD const & rect, bool doForceUpdate = false);

  void SaveState();
  bool LoadState();

  /// Resize event from window.
  virtual void OnSize(int w, int h);

  bool SetUpdatesEnabled(bool doEnable);

  int GetDrawScale() const;

  m2::PointD const & GetViewportCenter() const;
  void SetViewportCenter(m2::PointD const & pt);
  shared_ptr<MoveScreenTask> SetViewportCenterAnimated(m2::PointD const & endPt);

  /// Set correct viewport, parse API, show balloon.
  bool ShowMapForURL(string const & url);

  bool NeedRedraw() const;
  void SetNeedRedraw(bool flag);

  inline void XorQueryMaxScaleMode()
  {
    m_queryMaxScaleMode = !m_queryMaxScaleMode;
    Invalidate(true);
  }

  inline void SetQueryMaxScaleMode(bool mode)
  {
    m_queryMaxScaleMode = mode;
    Invalidate(true);
  }

  /// Get classificator types for nearest features.
  /// @param[in] pxPoint Current touch point in device pixel coordinates.
  void GetFeatureTypes(m2::PointD const & pxPoint, vector<string> & types) const;

  /// Get address information for point on map.
  inline void GetAddressInfoForPixelPoint(m2::PointD const & pxPoint, search::AddressInfo & info) const
  {
    GetAddressInfoForGlobalPoint(PtoG(pxPoint), info);
  }
  void GetAddressInfoForGlobalPoint(m2::PointD const & pt, search::AddressInfo & info) const;

private:
  void GetAddressInfo(FeatureType const & ft, m2::PointD const & pt, search::AddressInfo & info) const;
  void GetLocality(m2::PointD const & pt, search::AddressInfo & info) const;

public:
  bool GetVisiblePOI(m2::PointD const & pxPoint, m2::PointD & pxPivot, search::AddressInfo & info, feature::FeatureMetadata & metadata) const;
  void FindClosestPOIMetadata(m2::PointD const & pt, feature::FeatureMetadata & metadata) const;

#ifndef USE_DRAPE
  virtual void BeginPaint(shared_ptr<PaintEvent> const & e);
  /// Function for calling from platform dependent-paint function.
  virtual void DoPaint(shared_ptr<PaintEvent> const & e);

  virtual void EndPaint(shared_ptr<PaintEvent> const & e);
#endif // USE_DRAPE

private:
  /// Always check rect in public function for minimal draw scale.
  void CheckMinGlobalRect(m2::RectD & rect) const;
  /// Check for minimal draw scale and maximal logical scale (country is not loaded).
  void CheckMinMaxVisibleScale(m2::RectD & rect, int maxScale = -1) const;

  m2::AnyRectD ToRotated(m2::RectD const & rect) const;
  void ShowRectFixed(m2::RectD const & rect);
  void ShowRectFixedAR(m2::AnyRectD const & rect);

public:
  /// Show rect for point and needed draw scale.
  void ShowRect(double lat, double lon, double zoom);

  /// Set navigator viewport by rect as-is (rect should be in screen viewport).
  void ShowRect(m2::RectD rect);

  /// - Use navigator rotate angle.
  /// - Check for fixed scales (rect scale matched on draw tile scale).
  void ShowRectEx(m2::RectD rect);
  /// - Check minimal visible scale according to downloaded countries.
  void ShowRectExVisibleScale(m2::RectD rect, int maxScale = -1);

  void MemoryWarning();
  void EnterBackground();
  void EnterForeground();

  /// Show all model by it's world rect.
  void ShowAll();

  /// @name Drag implementation.
  //@{
public:
  m2::PointD GetPixelCenter() const;

  void StartDrag(DragEvent const & e);
  void DoDrag(DragEvent const & e);
  void StopDrag(DragEvent const & e);
  void Move(double azDir, double factor);
  //@}

  /// @name Rotation implementation
  //@{
  void StartRotate(RotateEvent const & e);
  void DoRotate(RotateEvent const & e);
  void StopRotate(RotateEvent const & e);
  //@}

  /// @name Scaling.
  //@{
  void ScaleToPoint(ScaleToPointEvent const & e, bool anim = true);
  void ScaleDefault(bool enlarge);
  void Scale(double scale);

private:
  void CalcScalePoints(ScaleEvent const & e, m2::PointD & pt1, m2::PointD & pt2) const;
public:
  void StartScale(ScaleEvent const & e);
  void DoScale(ScaleEvent const & e);
  void StopScale(ScaleEvent const & e);
  //@}

  gui::Controller * GetGuiController() const;
  anim::Controller * GetAnimController() const;

  Animator & GetAnimator();
  Navigator & GetNavigator();

  /// Set the localized strings bundle
  inline void AddString(string const & name, string const & value)
  {
    m_stringsBundle.SetString(name, value);
  }

  bool IsBenchmarking() const;

  StringsBundle const & GetStringsBundle();

  PinClickManager & GetBalloonManager() { return m_balloonManager; }

  /// Checks, whether the country which contains
  /// the specified point is loaded
  bool IsCountryLoaded(m2::PointD const & pt) const;

  shared_ptr<location::State> const & GetLocationState() const;
  void ActivateUserMark(UserMark const * mark, bool needAnim = true);
  bool HasActiveUserMark() const;
  UserMark const * GetUserMarkWithoutLogging(m2::PointD const & pxPoint, bool isLongPress);
  UserMark const * GetUserMark(m2::PointD const & pxPoint, bool isLongPress);
  PoiMarkPoint * GetAddressMark(m2::PointD const & globalPoint) const;
  BookmarkAndCategory FindBookmark(UserMark const * mark) const;

  /// [in] lat, lon - last known location
  /// [out] lat, lon - predicted location
  static void PredictLocation(double & lat, double & lon, double accuracy,
                              double bearing, double speed, double elapsedSeconds);

public:
  string CodeGe0url(Bookmark const * bmk, bool addName);
  string CodeGe0url(double lat, double lon, double zoomLevel, string const & name);

  /// @name Api
  //@{
  string GenerateApiBackUrl(ApiMarkPoint const & point);
  url_scheme::ParsedMapApi const & GetApiDataHolder() const { return m_ParsedMapApi; }

private:
  url_scheme::ParsedMapApi m_ParsedMapApi;
  void SetViewPortASync(m2::RectD const & rect);

  void UpdateSelectedMyPosition(m2::PointD const & pt);
  void DisconnectMyPositionUpdate();
  int m_locationChangedSlotID;

public:
  //@}

  /// @name Map updates
  //@{
  bool IsDataVersionUpdated();
  void UpdateSavedDataVersion();
  //@}

  BookmarkManager & GetBookmarkManager() { return m_bmManager; }

public:
  /// @name Routing mode
  //@{
  bool IsRoutingActive() const;
  bool IsRouteBuilt() const;
  void BuildRoute(m2::PointD const & destination);
  typedef function<void (routing::IRouter::ResultCode, vector<storage::TIndex> const &)> TRouteBuildingCallback;
  void SetRouteBuildingListener(TRouteBuildingCallback const & callback);
  void FollowRoute();
  void CloseRouting();
  void GetRouteFollowingInfo(location::FollowingInfo & info) const;

private:
  void RemoveRoute();
  void InsertRoute(routing::Route const & route);
  void CheckLocationForRouting(location::GpsInfo const & info);
  void MatchLocationToRoute(location::GpsInfo & info, location::RouteMatchingInfo & routeMatchingInfo) const;
  void CallRouteBuilded(routing::IRouter::ResultCode code, vector<storage::TIndex> const & absentFiles);
  string GetRoutingErrorMessage(routing::IRouter::ResultCode code);

  TRouteBuildingCallback m_routingCallback;
  //@}
};
