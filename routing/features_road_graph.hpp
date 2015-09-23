#pragma once

#include "road_graph.hpp"

#include "../indexer/feature_data.hpp"
#include "../geometry/point2d.hpp"
#include "../std/scoped_ptr.hpp"
#include "../std/vector.hpp"
#include "../base/cache.hpp"


class Index;
class FeatureType;


namespace routing
{

class IVehicleModel;

class FeaturesRoadGraph : public IRoadGraph
{
public:
  struct CachedFeature
  {
    CachedFeature() : m_speed(0), m_isOneway(false) {}

    vector<m2::PointD> m_points;
    double m_speed;
    bool m_isOneway;
  };

public:
  FeaturesRoadGraph(Index const * pIndex, size_t mwmID);

  virtual void GetPossibleTurns(RoadPos const & pos, vector<PossibleTurn> & turns, bool noOptimize = true);
  virtual void ReconstructPath(RoadPosVectorT const & positions, Route & route);

  static uint32_t GetStreetReadScale();

  inline size_t GetMwmID() const { return m_mwmID; }

  CachedFeature const & GetCachedFeature(uint32_t const ftId);

  double GetCacheMiss() const { return (double)m_cacheMiss / (double)m_cacheAccess; }

private:
  friend class CrossFeaturesLoader;

  bool IsOneWay(FeatureType const & ft) const;
  double GetSpeed(FeatureType const & ft) const;
  void LoadFeature(uint32_t id, FeatureType & ft);

private:
  Index const * m_pIndex;
  size_t m_mwmID;
  scoped_ptr<IVehicleModel> m_vehicleModel;
  my::Cache<uint32_t, CachedFeature> m_cache;

  uint32_t m_cacheMiss;
  uint32_t m_cacheAccess;
};

} // namespace routing
