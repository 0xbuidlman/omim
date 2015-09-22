#include "search_engine.hpp"
#include "result.hpp"
#include "search_query.hpp"

#include "../storage/country_info.hpp"

#include "../indexer/categories_holder.hpp"
#include "../indexer/search_string_utils.hpp"
#include "../indexer/mercator.hpp"
#include "../indexer/scales.hpp"

#include "../platform/platform.hpp"

#include "../geometry/distance_on_sphere.hpp"

#include "../base/logging.hpp"
#include "../base/stl_add.hpp"

#include "../std/map.hpp"
#include "../std/vector.hpp"
#include "../std/bind.hpp"


namespace search
{

typedef vector<Query::SuggestT> SuggestsContainerT;

class EngineData
{
public:
  EngineData(Reader * pCategoriesR, ModelReaderPtr polyR, ModelReaderPtr countryR)
    : m_categories(pCategoriesR), m_infoGetter(polyR, countryR)
  {
  }

  CategoriesHolder m_categories;
  SuggestsContainerT m_stringsToSuggest;
  storage::CountryInfoGetter m_infoGetter;
};

namespace
{

class InitSuggestions
{
  // Key - is a string with language.
  typedef map<pair<strings::UniString, int8_t>, uint8_t> SuggestMapT;
  SuggestMapT m_suggests;

public:
  void operator() (CategoriesHolder::Category::Name const & name)
  {
    if (name.m_prefixLengthToSuggest != CategoriesHolder::Category::EMPTY_PREFIX_LENGTH)
    {
      strings::UniString const uniName = NormalizeAndSimplifyString(name.m_name);

      uint8_t & score = m_suggests[make_pair(uniName, name.m_lang)];
      if (score == 0 || score > name.m_prefixLengthToSuggest)
        score = name.m_prefixLengthToSuggest;
    }
  }

  void GetSuggests(SuggestsContainerT & cont) const
  {
    cont.reserve(m_suggests.size());
    for (SuggestMapT::const_iterator i = m_suggests.begin(); i != m_suggests.end(); ++i)
      cont.push_back(Query::SuggestT(i->first.first, i->second, i->first.second));
  }
};

}


Engine::Engine(IndexType const * pIndex, Reader * pCategoriesR,
               ModelReaderPtr polyR, ModelReaderPtr countryR,
               string const & lang)
  : m_readyThread(false),
    m_pIndex(pIndex),
    m_pData(new EngineData(pCategoriesR, polyR, countryR))
{
  InitSuggestions doInit;
  m_pData->m_categories.ForEachName(bind<void>(ref(doInit), _1));
  doInit.GetSuggests(m_pData->m_stringsToSuggest);

  m_pQuery.reset(new Query(pIndex,
                           &m_pData->m_categories,
                           &m_pData->m_stringsToSuggest,
                           &m_pData->m_infoGetter,
                           RESULTS_COUNT));
  m_pQuery->SetPreferredLanguage(lang);
}

Engine::~Engine()
{
}

namespace
{
  m2::PointD GetViewportXY(double lat, double lon)
  {
    return m2::PointD(MercatorBounds::LonToX(lon), MercatorBounds::LatToY(lat));
  }
  m2::RectD GetViewportRect(double lat, double lon, double radius = 20000)
  {
    return MercatorBounds::MetresToXY(lon, lat, radius);
  }

  enum { VIEWPORT_RECT = 0, NEARME_RECT = 1 };

  // X metres in mercator (lon, lat) units.
  double const epsEqualRects = 100.0 * MercatorBounds::degreeInMetres;
  double const epsEqualPoints = 500.0 * MercatorBounds::degreeInMetres;

  /// Check rects for optimal search (avoid duplicating).
  void AnalyzeRects(m2::RectD arrRects[2])
  {
    if (arrRects[NEARME_RECT].IsRectInside(arrRects[VIEWPORT_RECT]) &&
        arrRects[NEARME_RECT].Center().EqualDxDy(arrRects[VIEWPORT_RECT].Center(), epsEqualRects))
    {
      arrRects[VIEWPORT_RECT].MakeEmpty();
    }
    else
    {
      if (arrRects[VIEWPORT_RECT].IsRectInside(arrRects[NEARME_RECT]) &&
          (scales::GetScaleLevel(arrRects[VIEWPORT_RECT]) + Query::SCALE_SEARCH_DEPTH >= scales::GetUpperScale()))
      {
        arrRects[NEARME_RECT].MakeEmpty();
      }
    }
  }
}

void Engine::PrepareSearch(m2::RectD const & viewport,
                           bool hasPt, double lat, double lon)
{
  m2::RectD const nearby = (hasPt ? GetViewportRect(lat, lon) : m2::RectD());

  // bind does copy of all rects
  GetPlatform().RunAsync(bind(&Engine::SetViewportAsync, this, viewport, nearby));
}

bool Engine::Search(SearchParams const & params, m2::RectD const & viewport)
{
  // Check for equal query.
  // There is no need to put synch here for reading m_params,
  // because this function is always called from main thread (one-by-one for queries).

  if (!params.IsForceSearch() &&
      m_params.IsEqualCommon(params) &&
      m2::IsEqual(m_viewport, viewport, epsEqualRects, epsEqualRects))
  {
    if (!m_params.IsValidPosition())
      return false;

    m2::PointD const p1 = GetViewportXY(m_params.m_lat, m_params.m_lon);
    m2::PointD const p2 = GetViewportXY(params.m_lat, params.m_lon);

    if (p1.EqualDxDy(p2, epsEqualPoints))
      return false;
  }

  {
    // Assign new search params.
    // Put the synch here, because this params are reading in search threads.
    threads::MutexGuard guard(m_updateMutex);
    m_params = params;
    m_viewport = viewport;
  }

  // Run task.
  GetPlatform().RunAsync(bind(&Engine::SearchAsync, this));

  return true;
}

void Engine::SetViewportAsync(m2::RectD const & viewport, m2::RectD const & nearby)
{
  // First of all - cancel previous query.
  m_pQuery->DoCancel();

  // Enter to run new search.
  threads::MutexGuard searchGuard(m_searchMutex);

  m2::RectD arrRects[] = { viewport, nearby };
  AnalyzeRects(arrRects);

  m_pQuery->SetViewport(arrRects, ARRAY_SIZE(arrRects));
}

void Engine::SearchAsync()
{
  {
    // Avoid many threads waiting in search mutex. One is enough.
    threads::MutexGuard readyGuard(m_readyMutex);
    if (m_readyThread)
      return;
    m_readyThread = true;
  }

  // First of all - cancel previous query.
  m_pQuery->DoCancel();

  // Enter to run new search.
  threads::MutexGuard searchGuard(m_searchMutex);

  {
    threads::MutexGuard readyGuard(m_readyMutex);
    m_readyThread = false;
  }

  // Get current search params.
  SearchParams params;
  m2::RectD arrRects[2];

  {
    threads::MutexGuard updateGuard(m_updateMutex);
    params = m_params;
    arrRects[VIEWPORT_RECT] = m_viewport;
  }

  // Initialize query.
  m_pQuery->Init();

  // Set search viewports according to search params.
  if (params.IsValidPosition())
  {
    if (params.NeedSearch(SearchParams::AROUND_POSITION))
    {
      m_pQuery->SetPosition(GetViewportXY(params.m_lat, params.m_lon));
      arrRects[NEARME_RECT] = GetViewportRect(params.m_lat, params.m_lon);
    }
  }
  else
    m_pQuery->NullPosition();

  if (!params.NeedSearch(SearchParams::IN_VIEWPORT))
    arrRects[VIEWPORT_RECT].MakeEmpty();

  if (arrRects[NEARME_RECT].IsValid() && arrRects[VIEWPORT_RECT].IsValid())
    AnalyzeRects(arrRects);

  m_pQuery->SetViewport(arrRects, 2);

  m_pQuery->SetSearchInWorld(params.NeedSearch(SearchParams::SEARCH_WORLD));

  if (params.IsLanguageValid())
    m_pQuery->SetInputLanguage(params.m_inputLanguageCode);

  m_pQuery->SetQuery(params.m_query);
  bool const emptyQuery = m_pQuery->IsEmptyQuery();

  Results res;

  // Call m_pQuery->IsCanceled() everywhere it needed without storing return value.
  // This flag can be changed from another thread.

  m_pQuery->SearchCoordinates(params.m_query, res);

  try
  {
    if (emptyQuery)
    {
      // Search for empty query only around viewport.
      if (params.IsValidPosition())
      {
        double arrR[] = { 500, 1000, 2000 };
        for (size_t i = 0; i < ARRAY_SIZE(arrR); ++i)
        {
          res.Clear();
          m_pQuery->SearchAllInViewport(GetViewportRect(params.m_lat, params.m_lon, arrR[i]),
                                        res, 3*RESULTS_COUNT);

          if (m_pQuery->IsCanceled() || res.GetCount() >= 2*RESULTS_COUNT)
            break;
        }
      }
    }
    else
      m_pQuery->Search(res);
  }
  catch (Query::CancelException const &)
  {
  }

  // Emit results even if search was canceled and we have something.
  size_t const count = res.GetCount();
  if (!m_pQuery->IsCanceled() || count > 0)
    params.m_callback(res);

  // Make additional search in whole mwm when not enough results (only for non-empty query).
  if (!emptyQuery && !m_pQuery->IsCanceled() && count < RESULTS_COUNT)
  {
    try
    {
      m_pQuery->SearchAdditional(res);
    }
    catch (Query::CancelException const &)
    {
    }

    // Emit if we have more results.
    if (res.GetCount() > count)
      params.m_callback(res);
  }

  // Emit finish marker to client.
  params.m_callback(Results::GetEndMarker(m_pQuery->IsCanceled()));
}

string Engine::GetCountryFile(m2::PointD const & pt)
{
  threads::MutexGuard searchGuard(m_searchMutex);

  return m_pData->m_infoGetter.GetRegionFile(pt);
}

string Engine::GetCountryCode(m2::PointD const & pt)
{
  threads::MutexGuard searchGuard(m_searchMutex);

  storage::CountryInfo info;
  m_pData->m_infoGetter.GetRegionInfo(pt, info);
  return info.m_flag;
}

template <class T> string Engine::GetCountryNameT(T const & t)
{
  threads::MutexGuard searchGuard(m_searchMutex);

  storage::CountryInfo info;
  m_pData->m_infoGetter.GetRegionInfo(t, info);
  return info.m_name;
}

string Engine::GetCountryName(m2::PointD const & pt)
{
  return GetCountryNameT(pt);
}

string Engine::GetCountryName(string const & id)
{
  return GetCountryNameT(id);
}

bool Engine::GetNameByType(uint32_t type, int8_t lang, string & name) const
{
  return m_pData->m_categories.GetNameByType(type, lang, name);
}

m2::RectD Engine::GetCountryBounds(string const & file) const
{
  return m_pData->m_infoGetter.CalcLimitRect(file);
}

int8_t Engine::GetCurrentLanguage() const
{
  return m_pQuery->GetPrefferedLanguage();
}

void Engine::ClearViewportsCache()
{
  threads::MutexGuard guard(m_searchMutex);

  m_pQuery->ClearCaches();
}

void Engine::ClearAllCaches()
{
  threads::MutexGuard guard(m_searchMutex);

  m_pQuery->ClearCaches();
  m_pData->m_infoGetter.ClearCaches();
}

}  // namespace search
