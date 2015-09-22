#pragma once

#include "road_graph_router.hpp"

#include "../std/map.hpp"

namespace routing
{
class DijkstraRouter : public RoadGraphRouter
{
  typedef RoadGraphRouter BaseT;

public:
  DijkstraRouter(Index const * pIndex = 0) : BaseT(pIndex) {}

  // IRouter overrides:
  string GetName() const override { return "pedestrian-dijkstra"; }

  // RoadGraphRouter overrides:
  ResultCode CalculateRouteM2M(vector<RoadPos> const & startPos, vector<RoadPos> const & finalPos,
                               vector<RoadPos> & route) override;
};
}  // namespace routing
