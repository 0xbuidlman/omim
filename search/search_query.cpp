#include "search_query.hpp"
#include "feature_offset_match.hpp"
#include "latlon_match.hpp"
#include "search_common.hpp"
#include "indexed_value.hpp"
#include "geometry_utils.hpp"
#include "search_string_intersection.hpp"

#include "../storage/country_info.hpp"

#include "../indexer/feature_impl.hpp"
#include "../indexer/feature_covering.hpp"
#include "../indexer/features_vector.hpp"
#include "../indexer/index.hpp"
#include "../indexer/scales.hpp"
#include "../indexer/search_delimiters.hpp"
#include "../indexer/search_string_utils.hpp"
#include "../indexer/categories_holder.hpp"
#include "../indexer/classificator.hpp"

#include "../coding/multilang_utf8_string.hpp"
#include "../coding/reader_wrapper.hpp"

#include "../base/logging.hpp"
#include "../base/string_utils.hpp"
#include "../base/stl_add.hpp"

#include "../std/algorithm.hpp"


namespace search
{

namespace
{
  typedef bool (*CompareFunctionT1) (impl::PreResult1 const &, impl::PreResult1 const &);
  typedef bool (*CompareFunctionT2) (impl::PreResult2 const &, impl::PreResult2 const &);

  CompareFunctionT1 g_arrCompare1[] =
  {
    &impl::PreResult1::LessRank,
    &impl::PreResult1::LessViewportDistance,
    &impl::PreResult1::LessDistance
  };

  CompareFunctionT2 g_arrCompare2[] =
  {
    &impl::PreResult2::LessRank,
    &impl::PreResult2::LessViewportDistance,
    &impl::PreResult2::LessDistance
  };

  /// This indexes should match the initialization routine below.
  int g_arrLang1[] = { 0, 1, 2, 2, 3 };
  int g_arrLang2[] = { 0, 0, 0, 1, 0 };
  enum LangIndexT { LANG_CURRENT = 0,
                    LANG_INPUT,
                    LANG_INTERNATIONAL,
                    LANG_EN,
                    LANG_DEFAULT,
                    LANG_COUNT };

  pair<int, int> GetLangIndex(int id)
  {
    ASSERT_LESS ( id, LANG_COUNT, () );
    return make_pair(g_arrLang1[id], g_arrLang2[id]);
  }

  // Maximum result candidates count for each viewport/criteria.
  size_t const PRE_RESULTS_COUNT = 200;
}

Query::Query(Index const * pIndex,
             CategoriesHolder const * pCategories,
             StringsToSuggestVectorT const * pStringsToSuggest,
             storage::CountryInfoGetter const * pInfoGetter)
  : m_pIndex(pIndex),
    m_pCategories(pCategories),
    m_pStringsToSuggest(pStringsToSuggest),
    m_pInfoGetter(pInfoGetter),
#ifdef HOUSE_SEARCH_TEST
    m_houseDetector(pIndex),
#endif
#ifdef FIND_LOCALITY_TEST
    m_locality(pIndex),
#endif
    m_worldSearch(true),
    m_sortByViewport(false),
    m_position(empty_pos_value, empty_pos_value)
{
  // m_viewport is initialized as empty rects

  ASSERT ( m_pIndex, () );

  // Results queue's initialization.
  STATIC_ASSERT ( QUEUES_COUNT == ARRAY_SIZE(g_arrCompare1) );
  STATIC_ASSERT ( QUEUES_COUNT == ARRAY_SIZE(g_arrCompare2) );

  for (size_t i = 0; i < QUEUES_COUNT; ++i)
  {
    m_results[i] = QueueT(PRE_RESULTS_COUNT, QueueCompareT(g_arrCompare1[i]));
    m_results[i].reserve(PRE_RESULTS_COUNT);
  }

  // Initialize keywords scorer.
  // Note! This order should match the indexes arrays above.
  vector<vector<int8_t> > langPriorities(4);
  langPriorities[0].push_back(-1);   // future current lang
  langPriorities[1].push_back(-1);   // future input lang
  langPriorities[2].push_back(StringUtf8Multilang::GetLangIndex("int_name"));
  langPriorities[2].push_back(StringUtf8Multilang::GetLangIndex("en"));
  langPriorities[3].push_back(StringUtf8Multilang::GetLangIndex("default"));
  m_keywordsScorer.SetLanguages(langPriorities);

  SetPreferredLanguage("en");
}

Query::~Query()
{
}

void Query::SetLanguage(int id, int8_t lang)
{
  m_keywordsScorer.SetLanguage(GetLangIndex(id), lang);
}

int8_t Query::GetLanguage(int id) const
{
  return m_keywordsScorer.GetLanguage(GetLangIndex(id));
}

namespace
{
  inline bool IsEqualMercator(m2::RectD const & r1, m2::RectD const & r2, double epsMeters)
  {
    double const eps = epsMeters * MercatorBounds::degreeInMetres;
    return m2::IsEqual(r1, r2, eps, eps);
  }
}

void Query::SetViewport(m2::RectD viewport[], size_t count)
{
  ASSERT(count < COUNT_V, (count));

  m_cancel = false;

  MWMVectorT mwmInfo;
  m_pIndex->GetMwmInfo(mwmInfo);

  for (size_t i = 0; i < count; ++i)
    SetViewportByIndex(mwmInfo, viewport[i], i);
}

void Query::SetViewportByIndex(MWMVectorT const & mwmInfo, m2::RectD const & viewport, size_t idx)
{
  ASSERT(idx < COUNT_V, (idx));

  if (viewport.IsValid())
  {
    // Check if viewports are equal (10 meters).
    if (!m_viewport[idx].IsValid() || !IsEqualMercator(m_viewport[idx], viewport, 10.0))
    {
      m_viewport[idx] = viewport;
      UpdateViewportOffsets(mwmInfo, viewport, m_offsetsInViewport[idx]);

#ifdef FIND_LOCALITY_TEST
      m_locality.SetViewportByIndex(viewport, idx);
#endif
    }
  }
  else
  {
    ClearCache(idx);

#ifdef FIND_LOCALITY_TEST
    m_locality.ClearCache(idx);
#endif
  }
}

void Query::SetPosition(m2::PointD const & pos)
{
  if (!m2::AlmostEqual(pos, m_position))
  {
    storage::CountryInfo ci;
    m_pInfoGetter->GetRegionInfo(pos, ci);
    m_region.swap(ci.m_name);
  }

  m_position = pos;
}

void Query::NullPosition()
{
  m_position = m2::PointD(empty_pos_value, empty_pos_value);
  m_region.clear();
}

void Query::SetPreferredLanguage(string const & lang)
{
  int8_t const code = StringUtf8Multilang::GetLangIndex(lang);
  SetLanguage(LANG_CURRENT, code);

  // Default initialization.
  // If you want to reset input language, call SetInputLanguage before search.
  SetInputLanguage(code);

#ifdef FIND_LOCALITY_TEST
  m_locality.SetLanguage(code);
#endif
}

void Query::SetInputLanguage(int8_t lang)
{
  LOG(LDEBUG, ("New input language = ", lang));
  SetLanguage(LANG_INPUT, lang);
}

int8_t Query::GetPrefferedLanguage() const
{
  return GetLanguage(LANG_CURRENT);
}

void Query::ClearCaches()
{
  for (size_t i = 0; i < COUNT_V; ++i)
    ClearCache(i);

  m_houseDetector.ClearCaches();

  m_locality.ClearCacheAll();
}

void Query::ClearCache(size_t ind)
{
  // clear cache and free memory
  OffsetsVectorT emptyV;
  emptyV.swap(m_offsetsInViewport[ind]);

  m_viewport[ind].MakeEmpty();
}

void Query::UpdateViewportOffsets(MWMVectorT const & mwmInfo, m2::RectD const & rect,
                                  OffsetsVectorT & offsets)
{
  offsets.clear();
  offsets.resize(mwmInfo.size());

  int const viewScale = scales::GetScaleLevel(rect);
  covering::CoveringGetter cov(rect, covering::ViewportWithLowLevels);

  for (MwmSet::MwmId mwmId = 0; mwmId < mwmInfo.size(); ++mwmId)
  {
    // Search only mwms that intersect with viewport (world always does).
    if (rect.IsIntersect(mwmInfo[mwmId].m_limitRect))
    {
      Index::MwmLock mwmLock(*m_pIndex, mwmId);
      if (MwmValue * pMwm = mwmLock.GetValue())
      {
        FHeaderT const & header = pMwm->GetHeader();
        if (header.GetType() == FHeaderT::country)
        {
          pair<int, int> const scaleR = header.GetScaleRange();
          int const scale = min(max(viewScale + SCALE_SEARCH_DEPTH, scaleR.first), scaleR.second);

          covering::IntervalsT const & interval = cov.Get(header.GetLastScale());

          ScaleIndex<ModelReaderPtr> index(pMwm->m_cont.GetReader(INDEX_FILE_TAG),
                                           pMwm->m_factory);

          for (size_t i = 0; i < interval.size(); ++i)
          {
            index.ForEachInIntervalAndScale(MakeBackInsertFunctor(offsets[mwmId]),
                                            interval[i].first, interval[i].second,
                                            scale);
          }

          sort(offsets[mwmId].begin(), offsets[mwmId].end());
        }
      }
    }
  }

#ifdef DEBUG
  size_t offsetsCached = 0;
  for (MwmSet::MwmId mwmId = 0; mwmId < mwmInfo.size(); ++mwmId)
    offsetsCached += offsets[mwmId].size();

  LOG(LDEBUG, ("For search in viewport cached ",
              "mwms:", mwmInfo.size(),
              "offsets:", offsetsCached));
#endif
}

void Query::Init(bool viewportPoints)
{
  m_cancel = false;

  m_tokens.clear();
  m_prefix.clear();

#ifdef HOUSE_SEARCH_TEST
  m_house.clear();
  m_streetID.clear();
#endif

  ClearQueues();

  if (viewportPoints)
  {
    m_queuesCount = 1;
    m_results[0] = QueueT(PRE_RESULTS_COUNT, QueueCompareT(&impl::PreResult1::LessPointsForViewport));
  }
  else
  {
    m_results[0] = QueueT(PRE_RESULTS_COUNT, QueueCompareT(g_arrCompare1[0]));
    m_queuesCount = QUEUES_COUNT;
  }
}

void Query::ClearQueues()
{
  for (size_t i = 0; i < QUEUES_COUNT; ++i)
    m_results[i].clear();
}

void Query::SetQuery(string const & query)
{
  m_query = &query;

  /// @todo Why Init is separated with SetQuery?
  ASSERT(m_tokens.empty(), ());
  ASSERT(m_prefix.empty(), ());
  ASSERT(m_house.empty(), ());

  // Split input query by tokens with possible last prefix.
  search::Delimiters delims;
  SplitUniString(NormalizeAndSimplifyString(query), MakeBackInsertFunctor(m_tokens), delims);

  bool checkPrefix = true;

  // Find most suitable token for house number.
#ifdef HOUSE_SEARCH_TEST
  int houseInd = static_cast<int>(m_tokens.size()) - 1;
  while (houseInd >= 0)
  {
    if (feature::IsHouseNumberDeepCheck(m_tokens[houseInd]))
    {
      if (m_tokens.size() > 1)
      {
        m_house.swap(m_tokens[houseInd]);
        m_tokens[houseInd].swap(m_tokens.back());
        m_tokens.pop_back();
      }
      break;
    }
    --houseInd;
  }

  // do check for prefix if last token is not a house number
  checkPrefix = m_house.empty() || houseInd < m_tokens.size();
#endif

  // Assign prefix with last parsed token.
  if (checkPrefix && !m_tokens.empty() && !delims(strings::LastUniChar(query)))
  {
    m_prefix.swap(m_tokens.back());
    m_tokens.pop_back();
  }

  int const maxTokensCount = MAX_TOKENS-1;
  if (m_tokens.size() > maxTokensCount)
    m_tokens.resize(maxTokensCount);

  // assign tokens and prefix to scorer
  m_keywordsScorer.SetKeywords(m_tokens.data(), m_tokens.size(), m_prefix);

  // get preffered types to show in results
  m_prefferedTypes.clear();
  if (m_pCategories)
  {
    for (size_t i = 0; i < m_tokens.size(); ++i)
      m_pCategories->ForEachTypeByName(m_tokens[i], MakeInsertFunctor(m_prefferedTypes));

    if (!m_prefix.empty())
      m_pCategories->ForEachTypeByName(m_prefix, MakeInsertFunctor(m_prefferedTypes));
  }
}

void Query::SearchCoordinates(string const & query, Results & res) const
{
  double lat, lon;
  if (MatchLatLonDegree(query, lat, lon))
  {
    //double const precision = 5.0 * max(0.0001, min(latPrec, lonPrec));  // Min 55 meters
    res.AddResult(MakeResult(impl::PreResult2(lat, lon)));
  }
}

void Query::Search(Results & res, size_t resCount)
{
  if (m_cancel) return;
  if (m_tokens.empty())
    SuggestStrings(res);

  if (m_cancel) return;
  SearchAddress(res);

  if (m_cancel) return;
  SearchFeatures();

  if (m_cancel) return;
  FlushResults(res, false, resCount);
}

namespace
{
  /// @name Functors to convert pointers to referencies.
  /// Pass them to stl algorithms.
  //@{
  template <class FunctorT> class ProxyFunctor1
  {
    FunctorT m_fn;
  public:
    template <class T> explicit ProxyFunctor1(T const & p) : m_fn(*p) {}
    template <class T> bool operator() (T const & p) { return m_fn(*p); }
  };

  template <class FunctorT> class ProxyFunctor2
  {
    FunctorT m_fn;
  public:
    template <class T> bool operator() (T const & p1, T const & p2)
    {
      return m_fn(*p1, *p2);
    }
  };
  //@}

  class IndexedValue : public search::IndexedValueBase<Query::QUEUES_COUNT>
  {
    typedef impl::PreResult2 ValueT;

    /// @todo Do not use shared_ptr for optimization issues.
    /// Need to rewrite std::unique algorithm.
    shared_ptr<ValueT> m_val;

  public:
    explicit IndexedValue(ValueT * v) : m_val(v) {}

    ValueT const & operator*() const { return *m_val; }

    string DebugPrint() const
    {
      string index;
      for (size_t i = 0; i < SIZE; ++i)
        index = index + " " + strings::to_string(m_ind[i]);

      return impl::DebugPrint(*m_val) + "; Index:" + index;
    }
  };

  inline string DebugPrint(IndexedValue const & t)
  {
    return t.DebugPrint();
  }

  struct CompFactory2
  {
    struct CompT
    {
      CompareFunctionT2 m_fn;
      explicit CompT(CompareFunctionT2 fn) : m_fn(fn) {}
      template <class T> bool operator() (T const & r1, T const & r2) const
      {
        return m_fn(*r1, *r2);
      }
    };

    static size_t const SIZE = 3;

    CompT Get(size_t i) { return CompT(g_arrCompare2[i]); }
  };

  struct LessFeatureID
  {
    typedef impl::PreResult1 ValueT;
    bool operator() (ValueT const & r1, ValueT const & r2) const
    {
      return (r1.GetID() < r2.GetID());
    }
  };

  class EqualFeatureID
  {
    typedef impl::PreResult1 ValueT;
    ValueT const & m_val;
  public:
    EqualFeatureID(ValueT const & v) : m_val(v) {}
    bool operator() (ValueT const & r) const
    {
      return (m_val.GetID() == r.GetID());
    }
  };

  template <class T>
  void RemoveDuplicatingLinear(vector<T> & indV)
  {
    sort(indV.begin(), indV.end(), ProxyFunctor2<impl::PreResult2::LessLinearTypesF>());
    indV.erase(unique(indV.begin(), indV.end(),
                      ProxyFunctor2<impl::PreResult2::EqualLinearTypesF>()),
               indV.end());
  }

  template <class T>
  bool IsResultExists(impl::PreResult2 const * p, vector<T> const & indV)
  {
    // do not insert duplicating results
    return (indV.end() != find_if(indV.begin(), indV.end(),
                                  ProxyFunctor1<impl::PreResult2::StrictEqualF>(p)));
  }
}

namespace impl
{
  class PreResult2Maker
  {
    Query & m_query;

    scoped_ptr<Index::FeaturesLoaderGuard> m_pFV;

    // For the best performance, incoming id's should be sorted by id.first (mwm file id).
    void LoadFeature(FeatureID const & id, FeatureType & f, string & name, string & country)
    {
      if (m_pFV.get() == 0 || m_pFV->GetID() != id.m_mwm)
        m_pFV.reset(new Index::FeaturesLoaderGuard(*m_query.m_pIndex, id.m_mwm));

      m_pFV->GetFeature(id.m_offset, f);
      f.SetID(id);

      m_query.GetBestMatchName(f, name);

      // country (region) name is a file name if feature isn't from World.mwm
      if (m_pFV->IsWorld())
        country.clear();
      else
        country = m_pFV->GetFileName();
    }

  public:
    PreResult2Maker(Query & q) : m_query(q)
    {
    }

    impl::PreResult2 * operator() (impl::PreResult1 const & res)
    {
      FeatureType feature;
      string name, country;
      LoadFeature(res.GetID(), feature, name, country);

      Query::ViewportID const viewportID = static_cast<Query::ViewportID>(res.GetViewportID());
      impl::PreResult2 * res2 = new impl::PreResult2(feature, &res,
                                                     m_query.GetViewport(viewportID), m_query.GetPosition(viewportID),
                                                     name, country);

      /// @todo: add exluding of states (without USA states), continents
      using namespace ftypes;
      Type const tp = IsLocalityChecker::Instance().GetType(res2->m_types);
      switch (tp)
      {
      case COUNTRY:
        res2->m_rank /= 1.5;
        break;
      case CITY:
      case TOWN:
        if (m_query.GetViewport(Query::CURRENT_V).IsPointInside(res2->GetCenter()))
          res2->m_rank *= 2;
        else
        {
          storage::CountryInfo ci;
          res2->m_region.GetRegion(m_query.m_pInfoGetter, ci);
          if (ci.IsNotEmpty() && ci.m_name == m_query.GetPositionRegion())
            res2->m_rank *= 1.7;
        }
        break;
      case VILLAGE:
        res2->m_rank /= 1.5;
        break;
      default:
        break;
      }

      return res2;
    }

    impl::PreResult2 * operator() (FeatureID const & id)
    {
      FeatureType feature;
      string name, country;
      LoadFeature(id, feature, name, country);

      if (!name.empty() && !country.empty())
      {
        return new impl::PreResult2(feature, 0,
                                    m_query.GetViewport(), m_query.GetPosition(),
                                    name, country);
      }
      else
        return 0;
    }
  };

  class HouseCompFactory
  {
    Query const & m_query;

    bool LessViewport(HouseResult const & r1, HouseResult const & r2) const
    {
      m2::RectD const & v = m_query.GetViewport();

      uint8_t const d1 = ViewportDistance(v, r1.GetOrg());
      uint8_t const d2 = ViewportDistance(v, r2.GetOrg());

      return (d1 != d2 ? d1 < d2 : LessDistance(r1, r2));
    }

    bool LessDistance(HouseResult const & r1, HouseResult const & r2) const
    {
      if (m_query.IsValidPosition())
      {
        return (PointDistance(m_query.m_position, r1.GetOrg()) <
                PointDistance(m_query.m_position, r2.GetOrg()));
      }
      else
        return false;
    }

  public:
    HouseCompFactory(Query const & q) : m_query(q) {}

    struct CompT
    {
      HouseCompFactory const * m_parent;
      size_t m_alg;

      CompT(HouseCompFactory const * parent, size_t alg) : m_parent(parent), m_alg(alg) {}
      bool operator() (HouseResult const & r1, HouseResult const & r2) const
      {
        if (m_alg == 0)
          return m_parent->LessViewport(r1, r2);
        else
          return m_parent->LessDistance(r1, r2);
      }
    };

    static size_t const SIZE = 2;

    CompT Get(size_t i) { return CompT(this, i); }
  };
}

template <class T> void Query::MakePreResult2(vector<T> & cont, vector<FeatureID> & streets)
{
  // make unique set of PreResult1
  typedef set<impl::PreResult1, LessFeatureID> PreResultSetT;
  PreResultSetT theSet;

  for (size_t i = 0; i < m_queuesCount; ++i)
  {
    theSet.insert(m_results[i].begin(), m_results[i].end());
    m_results[i].clear();
  }

  // make PreResult2 vector
  impl::PreResult2Maker maker(*this);
  for (PreResultSetT::const_iterator i = theSet.begin(); i != theSet.end(); ++i)
  {
    impl::PreResult2 * p = maker(*i);
    if (p == 0)
      continue;

    if (p->IsStreet())
      streets.push_back(p->GetID());

    if (IsResultExists(p, cont))
      delete p;
    else
      cont.push_back(IndexedValue(p));
  }
}

void Query::FlushHouses(Results & res, bool allMWMs, vector<FeatureID> const & streets)
{
  if (!m_house.empty() && !streets.empty())
  {
    if (m_houseDetector.LoadStreets(streets) > 0)
      m_houseDetector.MergeStreets();

    m_houseDetector.ReadAllHouses();

    vector<HouseResult> houses;
    m_houseDetector.GetHouseForName(strings::ToUtf8(m_house), houses);

    SortByIndexedValue(houses, impl::HouseCompFactory(*this));

    // Limit address results when searching in first pass (position, viewport, locality).
    size_t count = houses.size();
    if (!allMWMs)
      count = min(count, size_t(5));

    bool (Results::*addFn)(Result const &) = allMWMs ?
          &Results::AddResultCheckExisting :
          &Results::AddResult;

    for (size_t i = 0; i < count; ++i)
    {
      House const * h = houses[i].m_house;
      (res.*addFn)(MakeResult(impl::PreResult2(h->GetPosition(),
                                               h->GetNumber() + ", " + houses[i].m_street->GetName(),
                                               m_houseDetector.GetBuildingType())));
    }
  }
}

void Query::FlushResults(Results & res, bool allMWMs, size_t resCount)
{
  vector<IndexedValue> indV;
  vector<FeatureID> streets;

  MakePreResult2(indV, streets);

  if (indV.empty())
    return;

  RemoveDuplicatingLinear(indV);

  SortByIndexedValue(indV, CompFactory2());

  // Do not process suggestions in additional search.
  if (!allMWMs)
    ProcessSuggestions(indV, res);

  bool (Results::*addFn)(Result const &) = allMWMs ?
        &Results::AddResultCheckExisting :
        &Results::AddResult;

#ifdef HOUSE_SEARCH_TEST
  FlushHouses(res, allMWMs, streets);
#endif

  // emit feature results
  size_t count = res.GetCount();
  for (size_t i = 0; i < indV.size() && count < resCount; ++i)
  {
    if (m_cancel) break;

    LOG(LDEBUG, (indV[i]));

    if ((res.*addFn)(MakeResult(*(indV[i]))))
      ++count;
  }
}

void Query::SearchViewportPoints(Results & res)
{
  if (m_cancel) return;
  SearchAddress(res);

  if (m_cancel) return;
  SearchFeatures();

  vector<IndexedValue> indV;
  vector<FeatureID> streets;

  MakePreResult2(indV, streets);

  if (indV.empty())
    return;

  RemoveDuplicatingLinear(indV);

#ifdef HOUSE_SEARCH_TEST
  FlushHouses(res, false, streets);
#endif

  for (size_t i = 0; i < indV.size(); ++i)
  {
    if (m_cancel) break;

    res.AddResult((*(indV[i])).GeneratePointResult(m_pCategories, &m_prefferedTypes, GetLanguage(LANG_CURRENT)));
  }
}

ftypes::Type Query::GetLocalityIndex(feature::TypesHolder const & types) const
{
  using namespace ftypes;

  // Inner logic of SearchAddress expects COUNTRY, STATE and CITY only.
  Type const type = IsLocalityChecker::Instance().GetType(types);
  switch (type)
  {
  case TOWN:
    return CITY;
  case VILLAGE:
    return NONE;
  default:
    return type;
  }
}

void Query::RemoveStringPrefix(string const & str, string & res) const
{
  search::Delimiters delims;
  // Find start iterator of prefix in input query.
  typedef utf8::unchecked::iterator<string::const_iterator> IterT;
  IterT iter(str.end());
  while (iter.base() != str.begin())
  {
    IterT prev = iter;
    --prev;

    if (delims(*prev))
      break;
    else
      iter = prev;
  }

  // Assign result with input string without prefix.
  res.assign(str.begin(), iter.base());
}

void Query::GetSuggestion(string const & name, string & suggest) const
{
  // Split result's name on tokens.
  search::Delimiters delims;
  vector<strings::UniString> vName;
  SplitUniString(NormalizeAndSimplifyString(name), MakeBackInsertFunctor(vName), delims);

  // Find tokens that already present in input query.
  vector<bool> tokensMatched(vName.size());
  bool prefixMatched = false;
  for (size_t i = 0; i < vName.size(); ++i)
  {
    if (find(m_tokens.begin(), m_tokens.end(), vName[i]) != m_tokens.end())
      tokensMatched[i] = true;
    else
      if (vName[i].size() >= m_prefix.size() &&
          StartsWith(vName[i].begin(), vName[i].end(), m_prefix.begin(), m_prefix.end()))
      {
        prefixMatched = true;
      }
  }

  // Name doesn't match prefix - do nothing.
  if (!prefixMatched)
    return;

  RemoveStringPrefix(*m_query, suggest);

  // Append unmatched result's tokens to the suggestion.
  for (size_t i = 0; i < vName.size(); ++i)
    if (!tokensMatched[i])
    {
      suggest += strings::ToUtf8(vName[i]);
      suggest += " ";
    }
}

template <class T> void Query::ProcessSuggestions(vector<T> & vec, Results & res) const
{
  if (m_prefix.empty())
    return;

  int added = 0;
  for (typename vector<T>::iterator i = vec.begin(); i != vec.end();)
  {
    impl::PreResult2 const & r = **i;

    ftypes::Type const type = GetLocalityIndex(r.GetTypes());
    if ((type == ftypes::COUNTRY || type == ftypes::CITY)  || r.IsStreet())
    {
      string suggest;
      GetSuggestion(r.GetName(), suggest);
      if (!suggest.empty() && added < MAX_SUGGESTS_COUNT)
      {
        Result rr(MakeResult(r), suggest);
        if (res.AddResultCheckExisting(rr))
          ++added;

        i = vec.erase(i);
        continue;
      }
    }
    ++i;
  }
}

void Query::AddResultFromTrie(TrieValueT const & val, size_t mwmID, ViewportID vID /*= DEFAULT_V*/)
{
  impl::PreResult1 res(FeatureID(mwmID, val.m_featureId), val.m_rank, val.m_pt,
                       GetPosition(vID), GetViewport(vID), vID);

  for (size_t i = 0; i < m_queuesCount; ++i)
  {
    // here can be the duplicates because of different language match (for suggest token)
    if (m_results[i].end() == find_if(m_results[i].begin(), m_results[i].end(), EqualFeatureID(res)))
      m_results[i].push(res);
  }
}

namespace impl
{

class BestNameFinder
{
  KeywordLangMatcher::ScoreT m_score;
  string & m_name;
  KeywordLangMatcher const & m_keywordsScorer;
public:
  BestNameFinder(string & name, KeywordLangMatcher const & keywordsScorer)
    : m_score(), m_name(name), m_keywordsScorer(keywordsScorer)
  {
  }

  bool operator()(signed char lang, string const & name)
  {
    KeywordLangMatcher::ScoreT const score = m_keywordsScorer.Score(lang, name);
    if (m_score < score)
    {
      m_score = score;
      m_name = name;
    }
    return true;
  }
};

}  // namespace search::impl

void Query::GetBestMatchName(FeatureType const & f, string & name) const
{
  impl::BestNameFinder bestNameFinder(name, m_keywordsScorer);
  (void)f.ForEachNameRef(bestNameFinder);
}


template <class IterT, class ValueT> class CombinedIter
{
  IterT m_i, m_end;
  ValueT const * m_val;

public:
  CombinedIter(IterT i, IterT end, ValueT const * val)
    : m_i(i), m_end(end), m_val(val)
  {
  }

  ValueT const & operator*() const
  {
    ASSERT( m_val != 0 || m_i != m_end, ("dereferencing of empty iterator") );
    if (m_i != m_end)
      return *m_i;

    return *m_val;
  }

  CombinedIter & operator++()
  {
    if (m_i != m_end)
      ++m_i;
    else
      m_val = 0;
    return *this;
  }

  bool operator==(CombinedIter const & other) const
  {
    return m_val == other.m_val && m_i == other.m_i;
  }

  bool operator!=(CombinedIter const & other) const
  {
    return m_val != other.m_val || m_i != other.m_i;
  }
};


class AssignHighlightRange
{
  Result & m_res;
public:
  AssignHighlightRange(Result & res)
    : m_res(res)
  {
  }

  void operator() (pair<uint16_t, uint16_t> const & range)
  {
    m_res.AddHighlightRange(range);
  }
};


Result Query::MakeResult(impl::PreResult2 const & r) const
{
  Result res = r.GenerateFinalResult(m_pInfoGetter, m_pCategories, &m_prefferedTypes, GetLanguage(LANG_CURRENT));
  MakeResultHighlight(res);

#ifdef FIND_LOCALITY_TEST
  if (ftypes::IsLocalityChecker::Instance().GetType(r.GetTypes()) == ftypes::NONE)
  {
    string city;
    m_locality.GetLocalityInViewport(res.GetFeatureCenter(), city);
    res.AppendCity(city);
  }
#endif

  return res;
}

void Query::MakeResultHighlight(Result & res) const
{
  typedef buffer_vector<strings::UniString, 32>::const_iterator IterT;
  CombinedIter<IterT, strings::UniString> beg(m_tokens.begin(), m_tokens.end(), m_prefix.empty() ? 0 : &m_prefix);
  CombinedIter<IterT, strings::UniString> end(m_tokens.end(), m_tokens.end(), 0);

  SearchStringTokensIntersectionRanges(res.GetString(), beg, end, AssignHighlightRange(res));
}

namespace impl
{

class FeatureLoader
{
  Query & m_query;
  size_t m_mwmID, m_count;
  Query::ViewportID m_viewportID;

public:
  FeatureLoader(Query & query, size_t mwmID, Query::ViewportID viewportID)
    : m_query(query), m_mwmID(mwmID), m_count(0), m_viewportID(viewportID)
  {
    if (query.m_sortByViewport)
      m_viewportID = Query::CURRENT_V;
  }

  void operator() (Query::TrieValueT const & value)
  {
    ++m_count;
    m_query.AddResultFromTrie(value, m_mwmID, m_viewportID);
  }

  size_t GetCount() const { return m_count; }
  void Reset() { m_count = 0; }
};

}

namespace
{
  typedef vector<strings::UniString> TokensVectorT;

  class DoInsertTypeNames
  {
    Classificator const & m_c;
    TokensVectorT & m_tokens;
    bool m_supportOldFormat;

    static int GetOldTypeFromIndex(size_t index)
    {
      // "building" has old type value = 70
      ASSERT_NOT_EQUAL(index, 70, ());

      switch (index)
      {
      case 156: return 4099;
      case 98: return 4163;
      case 374: return 4419;
      case 188: return 4227;
      case 100: return 6147;
      case 107: return 4547;
      case 96: return 5059;
      case 60: return 6275;
      case 66: return 5251;
      case 161: return 4120;
      case 160: return 4376;
      case 159: return 4568;
      case 16: return 4233;
      case 178: return 5654;
      case 227: return 4483;
      case 111: return 5398;
      case 256: return 5526;
      case 702: return 263446;
      case 146: return 4186;
      case 155: return 4890;
      case 141: return 4570;
      case 158: return 4762;
      case 38: return 5891;
      case 63: return 4291;
      case 270: return 4355;
      case 327: return 4675;
      case 704: return 4611;
      case 242: return 4739;
      case 223: return 4803;
      case 174: return 4931;
      case 137: return 5123;
      case 186: return 5187;
      case 250: return 5315;
      case 104: return 4299;
      case 113: return 5379;
      case 206: return 4867;
      case 184: return 5443;
      case 125: return 5507;
      case 170: return 5571;
      case 25: return 5763;
      case 118: return 5827;
      case 76: return 6019;
      case 116: return 6083;
      case 108: return 6211;
      case 35: return 6339;
      case 180: return 6403;
      case 121: return 6595;
      case 243: return 6659;
      case 150: return 6723;
      case 175: return 6851;
      case 600: return 4180;
      case 348: return 4244;
      case 179: return 4116;
      case 77: return 4884;
      case 387: return 262164;
      case 214: return 4308;
      case 289: return 4756;
      case 264: return 4692;
      case 93: return 4500;
      case 240: return 4564;
      case 127: return 4820;
      case 29: return 4436;
      case 20: return 4948;
      case 18: return 4628;
      case 293: return 4372;
      case 22: return 4571;
      case 3: return 4699;
      case 51: return 4635;
      case 89: return 4123;
      case 307: return 5705;
      case 15: return 5321;
      case 6: return 4809;
      case 58: return 6089;
      case 26: return 5513;
      case 187: return 5577;
      case 1: return 5769;
      case 12: return 5897;
      case 244: return 5961;
      case 8: return 6153;
      case 318: return 6217;
      case 2: return 6025;
      case 30: return 5833;
      case 7: return 6281;
      case 65: return 6409;
      case 221: return 6473;
      case 54: return 4937;
      case 69: return 5385;
      case 4: return 6537;
      case 200: return 5257;
      case 195: return 5129;
      case 120: return 5193;
      case 56: return 5904;
      case 5: return 6864;
      case 169: return 4171;
      case 61: return 5707;
      case 575: return 5968;
      case 563: return 5456;
      case 13: return 6992;
      case 10: return 4811;
      case 109: return 4236;
      case 67: return 4556;
      case 276: return 4442;
      case 103: return 4506;
      case 183: return 4440;
      case 632: return 4162;
      case 135: return 4098;
      case 205: return 5004;
      case 87: return 4684;
      case 164: return 4940;
      case 201: return 4300;
      case 68: return 4620;
      case 101: return 5068;
      case 0: return 70;
      case 737: return 4102;
      case 703: return 5955;
      case 705: return 6531;
      case 706: return 5635;
      case 707: return 5699;
      case 708: return 4995;
      case 715: return 4298;
      case 717: return 4362;
      case 716: return 4490;
      case 718: return 4234;
      case 719: return 4106;
      case 722: return 4240;
      case 723: return 6480;
      case 725: return 4312;
      case 726: return 4248;
      case 727: return 4184;
      case 728: return 4504;
      case 732: return 4698;
      case 733: return 4378;
      case 734: return 4634;
      case 166: return 4250;
      case 288: return 4314;
      case 274: return 4122;
      }
      return -1;
    }

  public:
    DoInsertTypeNames(TokensVectorT & tokens, bool supportOldFormat)
      : m_c(classif()), m_tokens(tokens), m_supportOldFormat(supportOldFormat)
    {
    }
    void operator() (uint32_t t)
    {
      uint32_t const index = m_c.GetIndexForType(t);
      m_tokens.push_back(FeatureTypeToString(index));

      // v2-version MWM has raw classificator types in search index prefix, so
      // do the hack: add synonyms for old convention if needed.
      if (m_supportOldFormat)
      {
        int const type = GetOldTypeFromIndex(index);
        if (type >= 0)
        {
          ASSERT ( type == 70 || type > 4000, (type));
          m_tokens.push_back(FeatureTypeToString(static_cast<uint32_t>(type)));
        }
      }
    }
  };
}  // namespace search::impl

Query::Params::Params(Query const & q, bool isLocalities/* = false*/)
{
  if (!q.m_prefix.empty())
    m_prefixTokens.push_back(q.m_prefix);

  size_t const tokensCount = q.m_tokens.size();
  m_tokens.resize(tokensCount);

  // Add normal tokens.
  for (size_t i = 0; i < tokensCount; ++i)
    m_tokens[i].push_back(q.m_tokens[i]);

  // Add names of categories (and synonyms).
  if (q.m_pCategories && !isLocalities)
  {
    for (size_t i = 0; i < tokensCount; ++i)
      q.m_pCategories->ForEachTypeByName(q.m_tokens[i], DoInsertTypeNames(m_tokens[i], q.m_supportOldFormat));

    if (!q.m_prefix.empty())
      q.m_pCategories->ForEachTypeByName(q.m_prefix, DoInsertTypeNames(m_prefixTokens, q.m_supportOldFormat));
  }

  FillLanguages(q);
}

void Query::Params::EraseTokens(vector<size_t> & eraseInds)
{
  eraseInds.erase(unique(eraseInds.begin(), eraseInds.end()), eraseInds.end());

  // fill temporary vector
  vector<TokensVectorT> newTokens;

  size_t skipI = 0;
  size_t const count = m_tokens.size();
  size_t const eraseCount = eraseInds.size();
  for (size_t i = 0; i < count; ++i)
  {
    if (skipI < eraseCount && eraseInds[skipI] == i)
    {
      ++skipI;
    }
    else
    {
      newTokens.push_back(TokensVectorT());
      newTokens.back().swap(m_tokens[i]);
    }
  }

  // assign to m_tokens
  newTokens.swap(m_tokens);

  if (skipI < eraseCount)
  {
    // it means that we need to skip prefix tokens
    ASSERT_EQUAL ( skipI+1, eraseCount, (eraseInds) );
    ASSERT_EQUAL ( eraseInds[skipI], count, (eraseInds) );
    m_prefixTokens.clear();
  }
}

template <class ToDo> void Query::Params::ForEachToken(ToDo toDo)
{
  size_t const count = m_tokens.size();
  for (size_t i = 0; i < count; ++i)
  {
    ASSERT ( !m_tokens[i].empty(), () );
    ASSERT ( !m_tokens[i].front().empty(), () );
    toDo(m_tokens[i].front(), i);
  }

  if (!m_prefixTokens.empty())
  {
    ASSERT ( !m_prefixTokens.front().empty(), () );
    toDo(m_prefixTokens.front(), count);
  }
}

string DebugPrint(Query::Params const & p)
{
  return ("Query::Params: Tokens = " + DebugPrint(p.m_tokens) +
          "; Prefixes = " + DebugPrint(p.m_prefixTokens));
}

namespace
{
  class DoStoreNumbers
  {
    vector<size_t> & m_vec;
  public:
    DoStoreNumbers(vector<size_t> & vec) : m_vec(vec) {}
    void operator() (Query::Params::StringT const & s, size_t i)
    {
      /// @todo Do smart filtering of house numbers and zipcodes.
      if (feature::IsNumber(s))
        m_vec.push_back(i);
    }
  };

  class DoAddStreetSynonyms
  {
    Query::Params & m_params;

    Query::Params::TokensVectorT & GetTokens(size_t i)
    {
      size_t const count = m_params.m_tokens.size();
      if (i < count)
        return m_params.m_tokens[i];
      else
      {
        ASSERT_EQUAL ( i, count, () );
        return m_params.m_prefixTokens;
      }
    }

    void AddSynonym(size_t i, string const & sym)
    {
      GetTokens(i).push_back(strings::MakeUniString(sym));
    }

  public:
    DoAddStreetSynonyms(Query::Params & params) : m_params(params) {}

    void operator() (Query::Params::StringT const & s, size_t i)
    {
      if (s.size() <= 2)
      {
        string const ss = strings::ToUtf8(strings::MakeLowerCase(s));

        // All synonyms should be lowercase!
        if (ss == "n")
          AddSynonym(i, "north");
        else if (ss == "w")
          AddSynonym(i, "west");
        else if (ss == "s")
          AddSynonym(i, "south");
        else if (ss == "e")
          AddSynonym(i, "east");
        else if (ss == "nw")
          AddSynonym(i, "northwest");
        else if (ss == "ne")
          AddSynonym(i, "northeast");
        else if (ss == "sw")
          AddSynonym(i, "southwest");
        else if (ss == "se")
          AddSynonym(i, "southeast");
      }
    }
  };
}

void Query::Params::ProcessAddressTokens()
{
  // 1. Do simple stuff - erase all number tokens.
  // Assume that USA street name numbers are end with "st, nd, rd, th" suffixes.

  vector<size_t> toErase;
  ForEachToken(DoStoreNumbers(toErase));
  EraseTokens(toErase);

  // 2. Add synonyms for N, NE, NW, etc.
  ForEachToken(DoAddStreetSynonyms(*this));
}

void Query::Params::FillLanguages(Query const & q)
{
  for (int i = 0; i < LANG_COUNT; ++i)
    m_langs.insert(q.GetLanguage(i));
}

namespace impl
{
  struct Locality
  {
    string m_name, m_enName;        ///< native name and english name of locality
    Query::TrieValueT m_value;
    vector<size_t> m_matchedTokens; ///< indexes of matched tokens for locality

    ftypes::Type m_type;
    double m_radius;

    Locality() : m_type(ftypes::NONE) {}
    Locality(Query::TrieValueT const & val, ftypes::Type type)
      : m_value(val), m_type(type), m_radius(0)
    {
    }

    bool IsValid() const
    {
      if (m_type != ftypes::NONE)
      {
        ASSERT ( !m_matchedTokens.empty(), () );
        return true;
      }
      else
        return false;
    }

    void Swap(Locality & rhs)
    {
      m_name.swap(rhs.m_name);
      m_enName.swap(rhs.m_enName);
      m_matchedTokens.swap(rhs.m_matchedTokens);

      using std::swap;
      swap(m_value, rhs.m_value);
      swap(m_type, rhs.m_type);
      swap(m_radius, rhs.m_radius);
    }

    bool operator< (Locality const & rhs) const
    {
      if (m_type != rhs.m_type)
        return (m_type < rhs.m_type);

      if (m_matchedTokens.size() != rhs.m_matchedTokens.size())
        return (m_matchedTokens.size() < rhs.m_matchedTokens.size());

      return (m_value.m_rank < rhs.m_value.m_rank);
    }

  private:
    class DoCount
    {
      size_t & m_count;
    public:
      DoCount(size_t & count) : m_count(count) { m_count = 0; }
      template <class T> void operator() (T const &) { ++m_count; }
    };

    bool IsFullNameMatched() const
    {
      size_t count;
      SplitUniString(NormalizeAndSimplifyString(m_name), DoCount(count), search::Delimiters());
      return (count <= m_matchedTokens.size());
    }

    typedef strings::UniString StringT;
    typedef buffer_vector<StringT, 32> TokensArrayT;

    size_t GetSynonymTokenLength(TokensArrayT const & tokens, StringT const & prefix) const
    {
      // check only one token as a synonym
      if (m_matchedTokens.size() == 1)
      {
        size_t const index = m_matchedTokens[0];
        if (index < tokens.size())
          return tokens[index].size();
        else
        {
          ASSERT_EQUAL ( index, tokens.size(), () );
          ASSERT ( !prefix.empty(), () );
          return prefix.size();
        }
      }

      return size_t(-1);
    }

  public:
    /// Check that locality is suitable for source input tokens.
    bool IsSuitable(TokensArrayT const & tokens, StringT const & prefix) const
    {
      bool const isMatched = IsFullNameMatched();

      // Do filtering of possible localities.
      using namespace ftypes;

      switch (m_type)
      {
      case STATE:   // we process USA, Canada states only for now
        // USA states has 2-symbol synonyms
        return (isMatched || GetSynonymTokenLength(tokens, prefix) == 2);

      case COUNTRY:
        // USA has synonyms: "US" or "USA"
        return (isMatched ||
                (m_enName == "usa" && GetSynonymTokenLength(tokens, prefix) <= 3) ||
                (m_enName == "uk" && GetSynonymTokenLength(tokens, prefix) == 2));

      case CITY:
        // need full name match for cities
        return isMatched;

      default:
        ASSERT ( false, () );
        return false;
      }
    }
  };

  void swap(Locality & r1, Locality & r2) { r1.Swap(r2); }

  string DebugPrint(Locality const & l)
  {
    string res("Locality: ");
    res += "Name: " + l.m_name;
    res += "; Name English: " + l.m_enName;
    res += "; Rank: " + ::DebugPrint(l.m_value.m_rank);
    res += "; Matched: " + ::DebugPrint(l.m_matchedTokens.size());
    return res;
  }

  struct Region
  {
    vector<size_t> m_ids;
    vector<size_t> m_matchedTokens;
    string m_enName;

    bool operator<(Region const & rhs) const
    {
      return (m_matchedTokens.size() < rhs.m_matchedTokens.size());
    }

    bool IsValid() const
    {
      if (!m_ids.empty())
      {
        ASSERT ( !m_matchedTokens.empty(), () );
        ASSERT ( !m_enName.empty(), () );
        return true;
      }
      else
        return false;
    }

    void Swap(Region & rhs)
    {
      m_ids.swap(rhs.m_ids);
      m_matchedTokens.swap(rhs.m_matchedTokens);
      m_enName.swap(rhs.m_enName);
    }
  };

  string DebugPrint(Region const & r)
  {
    string res("Region: ");
    res += "Name English: " + r.m_enName;
    res += "; Matched: " + ::DebugPrint(r.m_matchedTokens.size());
    return res;
  }
}

void Query::SearchAddress(Results & res)
{
  // Find World.mwm and do special search there.
  MWMVectorT mwmInfo;
  m_pIndex->GetMwmInfo(mwmInfo);

  for (MwmSet::MwmId mwmId = 0; mwmId < mwmInfo.size(); ++mwmId)
  {
    Index::MwmLock mwmLock(*m_pIndex, mwmId);
    MwmValue * pMwm = mwmLock.GetValue();
    if (pMwm &&
        pMwm->m_cont.IsReaderExist(SEARCH_INDEX_FILE_TAG) &&
        pMwm->GetHeader().GetType() == FHeaderT::world)
    {
      impl::Locality city;
      impl::Region region;
      SearchLocality(pMwm, city, region);

      if (city.IsValid())
      {
        LOG(LDEBUG, ("Final city-locality = ", city));

        Params params(*this);
        params.EraseTokens(city.m_matchedTokens);

        if (params.CanSuggest())
          SuggestStrings(res);

        if (!params.IsEmpty())
        {
          params.ProcessAddressTokens();

          m2::RectD const rect = MercatorBounds::RectByCenterXYAndSizeInMeters(city.m_value.m_pt, city.m_radius);
          SetViewportByIndex(mwmInfo, rect, LOCALITY_V);

          /// @todo Hack - do not search for address in World.mwm; Do it better in future.
          bool const b = m_worldSearch;
          m_worldSearch = false;
          SearchFeatures(params, mwmInfo, LOCALITY_V);
          m_worldSearch = b;
        }
        else
        {
          // Add found locality as a result if nothing left to match.
          AddResultFromTrie(city.m_value, mwmId);
        }
      }
      else if (region.IsValid())
      {
        LOG(LDEBUG, ("Final region-locality = ", region));

        Params params(*this);
        params.EraseTokens(region.m_matchedTokens);

        if (params.CanSuggest())
          SuggestStrings(res);

        if (!params.IsEmpty())
        {
          for (MwmSet::MwmId id = 0; id < mwmInfo.size(); ++id)
          {
            Index::MwmLock mwmLock(*m_pIndex, id);
            if (m_pInfoGetter->IsBelongToRegion(mwmLock.GetFileName(), region.m_ids))
              SearchInMWM(mwmLock, params);
          }
        }
      }

      return;
    }
  }
}

namespace impl
{
  class DoFindLocality
  {
    Query const & m_query;

    /// Index in array equal to Locality::m_type value.
    vector<Locality> m_localities[3];

    FeaturesVector m_vector;
    size_t m_index;         ///< index of processing token

    Locality * PushLocality(Locality const & l)
    {
      ASSERT ( 0 <= l.m_type && l.m_type <= ARRAY_SIZE(m_localities), (l.m_type) );
      m_localities[l.m_type].push_back(l);
      return &(m_localities[l.m_type].back());
    }

    int8_t m_lang;
    int8_t m_arrEn[3];

    /// Tanslates country full english name to mwm file name prefix
    /// (need when matching correspondent mwm file in CountryInfoGetter::GetMatchedRegions).
    //@{
    static bool FeatureName2FileNamePrefix(string & name, char const * prefix,
                                    char const * arr[], size_t n)
    {
      for (size_t i = 0; i < n; ++i)
        if (name.find(arr[i]) == string::npos)
          return false;

      name = prefix;
      return true;
    }

    void AssignEnglishName(FeatureType const & f, Locality & l)
    {
      // search for name in next order: "en", "int_name", "default"
      for (int i = 0; i < 3; ++i)
        if (f.GetName(m_arrEn[i], l.m_enName))
        {
          // make name lower-case
          strings::AsciiToLower(l.m_enName);

          char const * arrUSA[] = { "united", "states", "america" };
          char const * arrUK[] = { "united", "kingdom" };

          if (!FeatureName2FileNamePrefix(l.m_enName, "usa", arrUSA, ARRAY_SIZE(arrUSA)))
            if (!FeatureName2FileNamePrefix(l.m_enName, "uk", arrUK, ARRAY_SIZE(arrUK)))

          return;
        }
    }
    //@}

    void AddRegions(int index, vector<Region> & regions) const
    {
      // fill regions vector in priority order
      vector<Locality> const & arr = m_localities[index];
      for (vector<Locality>::const_reverse_iterator i = arr.rbegin(); i != arr.rend(); ++i)
      {
        // no need to check region with empty english name (can't match for polygon)
        if (!i->m_enName.empty() && i->IsSuitable(m_query.m_tokens, m_query.m_prefix))
        {
          vector<size_t> vec;
          m_query.m_pInfoGetter->GetMatchedRegions(i->m_enName, vec);
          if (!vec.empty())
          {
            regions.push_back(Region());
            Region & r = regions.back();

            r.m_ids.swap(vec);
            r.m_matchedTokens = i->m_matchedTokens;
            r.m_enName = i->m_enName;
          }
        }
      }
    }

    bool IsBelong(Locality const & loc, Region const & r) const
    {
      // check that locality and region are produced from different tokens
      vector<size_t> dummy;
      set_intersection(loc.m_matchedTokens.begin(), loc.m_matchedTokens.end(),
                       r.m_matchedTokens.begin(), r.m_matchedTokens.end(),
                       back_inserter(dummy));

      if (dummy.empty())
      {
        // check that locality belong to region
        return m_query.m_pInfoGetter->IsBelongToRegion(loc.m_value.m_pt, r.m_ids);
      }

      return false;
    }

    class EqualID
    {
      uint32_t m_id;
    public:
      EqualID(uint32_t id) : m_id(id) {}
      bool operator() (Locality const & l) const { return (l.m_value.m_featureId == m_id); }
    };

  public:
    DoFindLocality(Query & q, MwmValue * pMwm, int8_t lang)
      : m_query(q), m_vector(pMwm->m_cont, pMwm->GetHeader()), m_lang(lang)
    {
      m_arrEn[0] = q.GetLanguage(LANG_EN);
      m_arrEn[1] = q.GetLanguage(LANG_INTERNATIONAL);
      m_arrEn[2] = q.GetLanguage(LANG_DEFAULT);
    }

    void Resize(size_t) {}
    void StartNew(size_t ind) { m_index = ind; }

    void operator() (Query::TrieValueT const & v)
    {
      if (m_query.IsCanceled())
        throw Query::CancelException();

      // find locality in current results
      for (size_t i = 0; i < 3; ++i)
      {
        vector<Locality>::iterator it = find_if(m_localities[i].begin(), m_localities[i].end(),
                                                EqualID(v.m_featureId));
        if (it != m_localities[i].end())
        {
          it->m_matchedTokens.push_back(m_index);
          return;
        }
      }

      // load feature
      FeatureType f;
      m_vector.Get(v.m_featureId, f);

      using namespace ftypes;

      // check, if feature is locality
      Type const index = m_query.GetLocalityIndex(feature::TypesHolder(f));
      if (index != NONE)
      {
        Locality * loc = PushLocality(Locality(v, index));
        if (loc)
        {
          loc->m_radius = GetRadiusByPopulation(GetPopulation(f));
          // m_lang name should exist if we matched feature in search index for this language.
          VERIFY(f.GetName(m_lang, loc->m_name), ());

          loc->m_matchedTokens.push_back(m_index);

          AssignEnglishName(f, *loc);
        }
      }
    }

    void SortLocalities()
    {
      for (int i = ftypes::COUNTRY; i <= ftypes::CITY; ++i)
        sort(m_localities[i].begin(), m_localities[i].end());
    }

    void GetRegions(vector<Region> & regions) const
    {
      //LOG(LDEBUG, ("Countries before processing = ", m_localities[ftypes::COUNTRY]));
      //LOG(LDEBUG, ("States before processing = ", m_localities[ftypes::STATE]));

      AddRegions(ftypes::STATE, regions);
      AddRegions(ftypes::COUNTRY, regions);

      //LOG(LDEBUG, ("Regions after processing = ", regions));
    }

    void GetBestCity(Locality & res, vector<Region> const & regions)
    {
      size_t const regsCount = regions.size();
      vector<Locality> & arr = m_localities[ftypes::CITY];

      // Interate in reverse order from better to generic locality.
      for (vector<Locality>::reverse_iterator i = arr.rbegin(); i != arr.rend(); ++i)
      {
        if (!i->IsSuitable(m_query.m_tokens, m_query.m_prefix))
          continue;

        // additional check for locality belongs to region
        vector<Region const *> belongs;
        for (size_t j = 0; j < regsCount; ++j)
        {
          if (IsBelong(*i, regions[j]))
            belongs.push_back(&regions[j]);
        }

        for (size_t j = 0; j < belongs.size(); ++j)
        {
          // splice locality info with region info
          i->m_matchedTokens.insert(i->m_matchedTokens.end(),
                                    belongs[j]->m_matchedTokens.begin(), belongs[j]->m_matchedTokens.end());
          // we need to store sorted range of token indexies
          sort(i->m_matchedTokens.begin(), i->m_matchedTokens.end());

          i->m_enName += (", " + belongs[j]->m_enName);
        }

        if (res < *i)
          i->Swap(res);

        if (regsCount == 0)
          return;
      }
    }
  };
}

void Query::SearchLocality(MwmValue * pMwm, impl::Locality & res1, impl::Region & res2)
{
  Params params(*this, true);

  serial::CodingParams cp(GetCPForTrie(pMwm->GetHeader().GetDefCodingParams()));

  ModelReaderPtr searchReader = pMwm->m_cont.GetReader(SEARCH_INDEX_FILE_TAG);
  scoped_ptr<TrieIterator> pTrieRoot(::trie::reader::ReadTrie(
                                     SubReaderWrapper<Reader>(searchReader.GetPtr()),
                                     trie::ValueReader(cp),
                                     trie::EdgeValueReader()));

  for (size_t i = 0; i < pTrieRoot->m_edge.size(); ++i)
  {
    TrieIterator::Edge::EdgeStrT const & edge = pTrieRoot->m_edge[i].m_str;

    /// We do search countries, states and cities for one language.
    /// @todo Do combine countries and cities for different languages.
    int8_t const lang = static_cast<int8_t>(edge[0]);
    if (edge[0] < search::CATEGORIES_LANG && params.IsLangExist(lang))
    {
      scoped_ptr<TrieIterator> pLangRoot(pTrieRoot->GoToEdge(i));

      // gel all localities from mwm
      impl::DoFindLocality doFind(*this, pMwm, lang);
      GetFeaturesInTrie(params.m_tokens, params.m_prefixTokens,
                        TrieRootPrefix(*pLangRoot, edge), doFind);

      // sort localities by priority
      doFind.SortLocalities();

      // get Region's from STATE and COUNTRY localities
      vector<impl::Region> regions;
      doFind.GetRegions(regions);

      // get best CITY locality
      impl::Locality loc;
      doFind.GetBestCity(loc, regions);
      if (res1 < loc)
      {
        LOG(LDEBUG, ("Better location ", loc, " for language ", lang));
        res1.Swap(loc);
      }

      // get best region
      if (!regions.empty())
      {
        sort(regions.begin(), regions.end());
        if (res2 < regions.back())
          res2.Swap(regions.back());
      }
    }
  }
}

void Query::SearchFeatures()
{
  MWMVectorT mwmInfo;
  m_pIndex->GetMwmInfo(mwmInfo);

  Params params(*this);

  // do usual search in viewport and near me (without last rect)
  for (int i = 0; i < LOCALITY_V; ++i)
  {
    if (m_viewport[i].IsValid())
      SearchFeatures(params, mwmInfo, static_cast<ViewportID>(i));
  }
}

namespace
{
  class FeaturesFilter
  {
    vector<uint32_t> const * m_offsets;

    volatile bool & m_isCancelled;
  public:
    FeaturesFilter(vector<uint32_t> const * offsets, volatile bool & isCancelled)
      : m_offsets(offsets), m_isCancelled(isCancelled)
    {
    }

    bool operator() (uint32_t offset) const
    {
      if (m_isCancelled)
      {
        //LOG(LINFO, ("Throw CancelException"));
        //dbg::ObjectTracker::PrintLeaks();
        throw Query::CancelException();
      }

      return (m_offsets == 0 ||
              binary_search(m_offsets->begin(), m_offsets->end(), offset));
    }
  };

  template <class FilterT> class TrieValuesHolder
  {
    vector<vector<Query::TrieValueT> > m_holder;
    size_t m_ind;
    FilterT const & m_filter;

  public:
    TrieValuesHolder(FilterT const & filter) : m_filter(filter) {}

    void Resize(size_t count) { m_holder.resize(count); }
    void StartNew(size_t ind)
    {
      ASSERT_LESS ( ind, m_holder.size(), () );
      m_ind = ind;
    }
    void operator() (Query::TrieValueT const & v)
    {
      if (m_filter(v.m_featureId))
        m_holder[m_ind].push_back(v);
    }

    template <class ToDo> void GetValues(size_t ind, ToDo & toDo) const
    {
      for (size_t i = 0; i < m_holder[ind].size(); ++i)
        toDo(m_holder[ind][i]);
    }
  };
}

void Query::SearchFeatures(Params const & params, MWMVectorT const & mwmInfo, ViewportID vID)
{
  for (MwmSet::MwmId mwmId = 0; mwmId < mwmInfo.size(); ++mwmId)
  {
    // Search only mwms that intersect with viewport (world always does).
    if (m_viewport[vID].IsIntersect(mwmInfo[mwmId].m_limitRect))
    {
      Index::MwmLock mwmLock(*m_pIndex, mwmId);
      SearchInMWM(mwmLock, params, vID);
    }
  }
}

namespace
{

void FillCategories(Query::Params const & params, TrieIterator const * pTrieRoot,
                    TrieValuesHolder<FeaturesFilter> & categoriesHolder)
{
  scoped_ptr<TrieIterator> pCategoriesRoot;
  typedef TrieIterator::Edge::EdgeStrT EdgeT;
  EdgeT categoriesEdge;

  size_t const count = pTrieRoot->m_edge.size();
  for (size_t i = 0; i < count; ++i)
  {
    EdgeT const & edge = pTrieRoot->m_edge[i].m_str;
    ASSERT_GREATER_OR_EQUAL(edge.size(), 1, ());

    if (edge[0] == search::CATEGORIES_LANG)
    {
      categoriesEdge = edge;
      pCategoriesRoot.reset(pTrieRoot->GoToEdge(i));
      break;
    }
  }
  ASSERT_NOT_EQUAL(pCategoriesRoot, 0, ());

  GetFeaturesInTrie(params.m_tokens, params.m_prefixTokens,
                    TrieRootPrefix(*pCategoriesRoot, categoriesEdge),
                    categoriesHolder);
}

}

void Query::SearchInMWM(Index::MwmLock const & mwmLock, Params const & params,
                        ViewportID vID /*= DEFAULT_V*/)
{
  if (MwmValue * pMwm = mwmLock.GetValue())
  {
    if (pMwm->m_cont.IsReaderExist(SEARCH_INDEX_FILE_TAG))
    {
      FHeaderT const & header = pMwm->GetHeader();

      /// @todo do not process World.mwm here - do it in SearchLocality
      bool const isWorld = (header.GetType() == FHeaderT::world);
      if (isWorld && !m_worldSearch)
        return;

      serial::CodingParams cp(GetCPForTrie(header.GetDefCodingParams()));

      ModelReaderPtr searchReader = pMwm->m_cont.GetReader(SEARCH_INDEX_FILE_TAG);
      scoped_ptr<TrieIterator> pTrieRoot(::trie::reader::ReadTrie(
                                         SubReaderWrapper<Reader>(searchReader.GetPtr()),
                                         trie::ValueReader(cp),
                                         trie::EdgeValueReader()));

      MwmSet::MwmId const mwmId = mwmLock.GetID();
      FeaturesFilter filter((vID == DEFAULT_V || isWorld) ? 0 : &m_offsetsInViewport[vID][mwmId], m_cancel);

      // Get categories for each token separately - find needed edge with categories.
      TrieValuesHolder<FeaturesFilter> categoriesHolder(filter);
      FillCategories(params, pTrieRoot.get(), categoriesHolder);

      // Match tokens to feature for each language - iterate through first edges.
      impl::FeatureLoader emitter(*this, mwmId, vID);
      size_t const count = pTrieRoot->m_edge.size();
      for (size_t i = 0; i < count; ++i)
      {
        TrieIterator::Edge::EdgeStrT const & edge = pTrieRoot->m_edge[i].m_str;
        int8_t const lang = static_cast<int8_t>(edge[0]);

        if (edge[0] < search::CATEGORIES_LANG && params.IsLangExist(lang))
        {
          scoped_ptr<TrieIterator> pLangRoot(pTrieRoot->GoToEdge(i));

          MatchFeaturesInTrie(params.m_tokens, params.m_prefixTokens,
                              TrieRootPrefix(*pLangRoot, edge),
                              filter, categoriesHolder, emitter);

          LOG(LDEBUG, ("Country", pMwm->GetFileName(),
                       "Lang", StringUtf8Multilang::GetLangByCode(lang),
                       "Matched", emitter.GetCount()));

          emitter.Reset();
        }
      }
    }
  }
}

void Query::SuggestStrings(Results & res)
{
  if (m_pStringsToSuggest && !m_prefix.empty())
  {
    // Match prefix.
    MatchForSuggestions(m_prefix, res);
  }
}

bool Query::MatchForSuggestionsImpl(strings::UniString const & token, int8_t lang, string const & prolog, Results & res)
{
  bool ret = false;

  StringsToSuggestVectorT::const_iterator it = m_pStringsToSuggest->begin();
  for (; it != m_pStringsToSuggest->end(); ++it)
  {
    strings::UniString const & s = it->m_name;
    if ((it->m_prefixLength <= token.size()) &&
        (token != s) &&          // do not push suggestion if it already equals to token
        (it->m_lang == lang) &&  // push suggestions only for needed language
        StartsWith(s.begin(), s.end(), token.begin(), token.end()))
    {
      string const utf8Str = strings::ToUtf8(s);
      Result r(utf8Str, prolog + utf8Str + " ");
      MakeResultHighlight(r);
      res.AddResult(r);
      ret = true;
    }
  }

  return ret;
}

void Query::MatchForSuggestions(strings::UniString const & token, Results & res)
{
  string prolog;
  RemoveStringPrefix(*m_query, prolog);

  if (!MatchForSuggestionsImpl(token, GetLanguage(LANG_INPUT), prolog, res))
    MatchForSuggestionsImpl(token, GetLanguage(LANG_EN), prolog, res);
}

m2::RectD const & Query::GetViewport(ViewportID vID /*= DEFAULT_V*/) const
{
  if (vID == LOCALITY_V)
  {
    // special case for search address - return viewport around location
    return m_viewport[vID];
  }

  // return first valid actual viewport
  if (m_viewport[0].IsValid())
    return m_viewport[0];
  else
  {
    ASSERT ( m_viewport[1].IsValid(), () );
    return m_viewport[1];
  }
}

m2::PointD Query::GetPosition(ViewportID vID /*= DEFAULT_V*/) const
{
  switch (vID)
  {
  case LOCALITY_V:      // center of the founded locality
    return m_viewport[vID].Center();

  case CURRENT_V:       // center of viewport for special sort mode
    if (m_sortByViewport)
      return m_viewport[vID].Center();

  default:
    return m_position;
  }
}

void Query::SearchAllInViewport(m2::RectD const & viewport, Results & res, unsigned int resultsNeeded)
{
  ASSERT ( viewport.IsValid(), () );

  // Get feature's offsets in viewport.
  OffsetsVectorT offsets;
  {
    MWMVectorT mwmInfo;
    m_pIndex->GetMwmInfo(mwmInfo);

    UpdateViewportOffsets(mwmInfo, viewport, offsets);
  }

  vector<shared_ptr<impl::PreResult2> > indV;

  impl::PreResult2Maker maker(*this);

  // load results
  for (size_t i = 0; i < offsets.size(); ++i)
  {
    if (m_cancel) break;

    for (size_t j = 0; j < offsets[i].size(); ++j)
    {
      if (m_cancel) break;

      impl::PreResult2 * p = maker(FeatureID(i, offsets[i][j]));
      if (p && !IsResultExists(p, indV))
        indV.push_back(make_shared_ptr(p));
      else
        delete p;
    }
  }

  if (!m_cancel)
  {
    RemoveDuplicatingLinear(indV);

    // sort by distance from m_position
    sort(indV.begin(), indV.end(), CompFactory2::CompT(&impl::PreResult2::LessDistance));

    // emit results
    size_t const count = min(indV.size(), static_cast<size_t>(resultsNeeded));
    for (size_t i = 0; i < count; ++i)
    {
      if (m_cancel) break;

      res.AddResult(MakeResult(*(indV[i])));
    }
  }
}

bool Query::IsValidPosition() const
{
  return (m_position.x > empty_pos_value && m_position.y > empty_pos_value);
}

void Query::SearchAdditional(Results & res, bool nearMe, bool inViewport, size_t resCount)
{
  ClearQueues();

  string name[2];

  // search in mwm with position ...
  if (nearMe && IsValidPosition())
    name[0] = m_pInfoGetter->GetRegionFile(m_position);

  // ... and in mwm with viewport
  if (inViewport)
    name[1] = m_pInfoGetter->GetRegionFile(GetViewport().Center());

  LOG(LDEBUG, ("Additional MWM search: ", name[0], name[1]));

  if (!(name[0].empty() && name[1].empty()))
  {
    MWMVectorT mwmInfo;
    m_pIndex->GetMwmInfo(mwmInfo);

    Params params(*this);

    for (MwmSet::MwmId mwmId = 0; mwmId < mwmInfo.size(); ++mwmId)
    {
      Index::MwmLock mwmLock(*m_pIndex, mwmId);
      string const s = mwmLock.GetFileName();

      if (s == name[0] || s == name[1])
        SearchInMWM(mwmLock, params);
    }

    FlushResults(res, true, resCount);
  }
}

}  // namespace search
