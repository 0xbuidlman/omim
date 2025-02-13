#pragma once

#include "search/intermediate_result.hpp"
#include "search/nested_rects_cache.hpp"
#include "search/ranker.hpp"

#include "indexer/index.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include "base/macros.hpp"

#include "std/algorithm.hpp"
#include "std/cstdint.hpp"
#include "std/random.hpp"
#include "std/set.hpp"
#include "std/utility.hpp"
#include "std/vector.hpp"

namespace search
{
// Fast and simple pre-ranker for search results.
class PreRanker
{
public:
  struct Params
  {
    m2::RectD m_viewport;
    double m_minDistanceOnMapBetweenResults = 0.0;

    // This is different from geocoder's pivot because pivot is
    // usually a rectangle created by radius and center and, due to
    // precision loss, its center may differ from
    // |m_accuratePivotCenter|. Therefore the pivot should be used for
    // fast filtering of features outside of the rectangle, while
    // |m_accuratePivotCenter| should be used when it's needed to
    // compute the distance from a feature to the pivot.
    m2::PointD m_accuratePivotCenter = m2::PointD(0, 0);
    int m_scale = 0;

    size_t m_batchSize = 100;
  };

  PreRanker(Index const & index, Ranker & ranker, size_t limit);

  void Init(Params const & params);

  inline void SetViewportSearch(bool viewportSearch) { m_viewportSearch = viewportSearch; }

  template <typename... TArgs>
  void Emplace(TArgs &&... args)
  {
    if (m_numSentResults >= m_limit)
      return;
    m_results.emplace_back(forward<TArgs>(args)...);
  }

  // Computes missing fields for all pre-results.
  void FillMissingFieldsInPreResults();

  void Filter(bool viewportSearch);

  // Emit a new batch of results up the pipeline (i.e. to ranker).
  // Use lastUpdate in geocoder to indicate that
  // no more results will be added.
  void UpdateResults(bool lastUpdate);

  inline size_t Size() const { return m_results.size(); }
  inline size_t BatchSize() const { return m_params.m_batchSize; }
  inline size_t NumSentResults() const { return m_numSentResults; }
  inline size_t Limit() const { return m_limit; }

  template <typename TFn>
  void ForEach(TFn && fn)
  {
    for_each(m_results.begin(), m_results.end(), forward<TFn>(fn));
  }

  void ClearCaches();

private:
  void FilterForViewportSearch();

  Index const & m_index;
  Ranker & m_ranker;
  vector<PreResult1> m_results;
  size_t const m_limit;
  Params m_params;
  bool m_viewportSearch = false;

  // Amount of results sent up the pipeline.
  size_t m_numSentResults = 0;

  // Cache of nested rects used to estimate distance from a feature to the pivot.
  NestedRectsCache m_pivotFeatures;

  // A set of ids for features that are emitted during the current search session.
  set<FeatureID> m_currEmit;

  // A set of ids for features that were emitted during the previous
  // search session.  They're used for filtering of current search in
  // viewport results, because we need to give more priority to
  // results that were on map previously.
  set<FeatureID> m_prevEmit;

  minstd_rand m_rng;

  DISALLOW_COPY_AND_MOVE(PreRanker);
};
}  // namespace search
