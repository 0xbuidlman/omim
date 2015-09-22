#include "road_graph_builder.hpp"

#include "../../std/algorithm.hpp"

using namespace routing;

namespace routing_test
{

void RoadInfo::Swap(RoadInfo & r)
{
  m_points.swap(r.m_points);
  std::swap(m_speedMS, r.m_speedMS);
  std::swap(m_bothSides, r.m_bothSides);
}

void RoadGraphMockSource::AddRoad(RoadInfo & rd)
{
  /// @todo Do ASSERT for RoadInfo params.

  uint32_t const id = m_roads.size();

  // Count of sections! (not points)
  size_t count = rd.m_points.size();
  ASSERT_GREATER(count, 1, ());

  for (size_t i = 0; i < count; ++i)
  {
    TurnsMapT::iterator j = m_turns.insert(make_pair(rd.m_points[i], TurnsVectorT())).first;

    PossibleTurn t;
    t.m_startPoint = rd.m_points.front();
    t.m_endPoint = rd.m_points.back();
    t.m_speed = rd.m_speedMS;

    if (i > 0)
    {
      t.m_pos = RoadPos(id, true, i - 1);
      j->second.push_back(t);
    }

    if (rd.m_bothSides && (i < count - 1))
    {
      t.m_pos = RoadPos(id, false, i);
      j->second.push_back(t);
    }
  }

  m_roads.push_back(RoadInfo());
  m_roads.back().Swap(rd);
}

void RoadGraphMockSource::GetPossibleTurns(RoadPos const & pos, TurnsVectorT & turns, bool /*noOptimize = true*/)
{
  uint32_t const fID = pos.GetFeatureId();
  ASSERT_LESS(fID, m_roads.size(), ());

  vector<m2::PointD> const & points = m_roads[fID].m_points;

  int const inc = pos.IsForward() ? -1 : 1;
  int startID = pos.GetPointId();
  int const count = static_cast<int>(points.size());

  if (!pos.IsForward())
    ++startID;

  double const speed = m_roads[fID].m_speedMS;

  double distance = 0.0;
  double time = 0.0;
  for (int i = startID; i >= 0 && i < count; i += inc)
  {
    double const len = points[i - inc].Length(points[i]);
    distance += len;
    time += len / speed;

    TurnsMapT::const_iterator j = m_turns.find(points[i]);
    if (j != m_turns.end())
    {
      vector<PossibleTurn> const & vec = j->second;
      for (size_t k = 0; k < vec.size(); ++k)
      {
        if (vec[k].m_pos.GetFeatureId() != pos.GetFeatureId() ||
            vec[k].m_pos.IsForward() == pos.IsForward())
        {
          PossibleTurn t = vec[k];

          t.m_metersCovered = distance;
          t.m_secondsCovered = time;
          turns.push_back(t);
        }
      }
    }
  }
}

void RoadGraphMockSource::ReconstructPath(RoadPosVectorT const &, routing::Route &)
{
}

void InitRoadGraphMockSourceWithTest1(RoadGraphMockSource & src)
{
  {
    RoadInfo ri;
    ri.m_bothSides = true;
    ri.m_speedMS = 40;
    ri.m_points.push_back(m2::PointD(0, 0));
    ri.m_points.push_back(m2::PointD(5, 0));
    ri.m_points.push_back(m2::PointD(10, 0));
    ri.m_points.push_back(m2::PointD(15, 0));
    ri.m_points.push_back(m2::PointD(20, 0));
    src.AddRoad(ri);
  }

  {
    RoadInfo ri;
    ri.m_bothSides = true;
    ri.m_speedMS = 40;
    ri.m_points.push_back(m2::PointD(10, -10));
    ri.m_points.push_back(m2::PointD(10, -5));
    ri.m_points.push_back(m2::PointD(10, 0));
    ri.m_points.push_back(m2::PointD(10, 5));
    ri.m_points.push_back(m2::PointD(10, 10));
    src.AddRoad(ri);
  }

  {
    RoadInfo ri;
    ri.m_bothSides = true;
    ri.m_speedMS = 40;
    ri.m_points.push_back(m2::PointD(15, -5));
    ri.m_points.push_back(m2::PointD(15, 0));
    src.AddRoad(ri);
  }

  {
    RoadInfo ri;
    ri.m_bothSides = true;
    ri.m_speedMS = 40;
    ri.m_points.push_back(m2::PointD(20, 0));
    ri.m_points.push_back(m2::PointD(25, 5));
    ri.m_points.push_back(m2::PointD(15, 5));
    ri.m_points.push_back(m2::PointD(20, 0));
    src.AddRoad(ri);
  }
}

void InitRoadGraphMockSourceWithTest2(RoadGraphMockSource & graph)
{
  RoadInfo ri0;
  ri0.m_bothSides = true;
  ri0.m_speedMS = 40;
  ri0.m_points.push_back(m2::PointD(0, 0));
  ri0.m_points.push_back(m2::PointD(10, 0));
  ri0.m_points.push_back(m2::PointD(25, 0));
  ri0.m_points.push_back(m2::PointD(35, 0));
  ri0.m_points.push_back(m2::PointD(70, 0));
  ri0.m_points.push_back(m2::PointD(80, 0));

  RoadInfo ri1;
  ri1.m_bothSides = false;
  ri1.m_speedMS = 40;
  ri1.m_points.push_back(m2::PointD(0, 0));
  ri1.m_points.push_back(m2::PointD(5, 10));
  ri1.m_points.push_back(m2::PointD(5, 40));

  RoadInfo ri2;
  ri2.m_bothSides = false;
  ri2.m_speedMS = 40;
  ri2.m_points.push_back(m2::PointD(12, 25));
  ri2.m_points.push_back(m2::PointD(10, 10));
  ri2.m_points.push_back(m2::PointD(10, 0));

  RoadInfo ri3;
  ri3.m_bothSides = true;
  ri3.m_speedMS = 40;
  ri3.m_points.push_back(m2::PointD(5, 10));
  ri3.m_points.push_back(m2::PointD(10, 10));
  ri3.m_points.push_back(m2::PointD(70, 10));
  ri3.m_points.push_back(m2::PointD(80, 10));

  RoadInfo ri4;
  ri4.m_bothSides = true;
  ri4.m_speedMS = 40;
  ri4.m_points.push_back(m2::PointD(25, 0));
  ri4.m_points.push_back(m2::PointD(27, 25));

  RoadInfo ri5;
  ri5.m_bothSides = true;
  ri5.m_speedMS = 40;
  ri5.m_points.push_back(m2::PointD(35, 0));
  ri5.m_points.push_back(m2::PointD(37, 30));
  ri5.m_points.push_back(m2::PointD(70, 30));
  ri5.m_points.push_back(m2::PointD(80, 30));

  RoadInfo ri6;
  ri6.m_bothSides = true;
  ri6.m_speedMS = 40;
  ri6.m_points.push_back(m2::PointD(70, 0));
  ri6.m_points.push_back(m2::PointD(70, 10));
  ri6.m_points.push_back(m2::PointD(70, 30));

  RoadInfo ri7;
  ri7.m_bothSides = true;
  ri7.m_speedMS = 40;
  ri7.m_points.push_back(m2::PointD(39, 55));
  ri7.m_points.push_back(m2::PointD(80, 55));

  RoadInfo ri8;
  ri8.m_bothSides = false;
  ri8.m_speedMS = 40;
  ri8.m_points.push_back(m2::PointD(5, 40));
  ri8.m_points.push_back(m2::PointD(18, 55));
  ri8.m_points.push_back(m2::PointD(39, 55));
  ri8.m_points.push_back(m2::PointD(37, 30));
  ri8.m_points.push_back(m2::PointD(27, 25));
  ri8.m_points.push_back(m2::PointD(12, 25));
  ri8.m_points.push_back(m2::PointD(5, 40));

  graph.AddRoad(ri0);
  graph.AddRoad(ri1);
  graph.AddRoad(ri2);
  graph.AddRoad(ri3);
  graph.AddRoad(ri4);
  graph.AddRoad(ri5);
  graph.AddRoad(ri6);
  graph.AddRoad(ri7);
  graph.AddRoad(ri8);
}


} // namespace routing_test
