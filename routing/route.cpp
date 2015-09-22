#include "route.hpp"

#include "../indexer/mercator.hpp"

#include "../platform/location.hpp"

#include "../geometry/distance_on_sphere.hpp"

#include "../base/logging.hpp"

#include "../std/numeric.hpp"


namespace routing
{

static double const LOCATION_TIME_THRESHOLD = 60.0*1.0;
static double const ON_ROAD_TOLERANCE_M = 20.0;
static double const ON_END_TOLERANCE_M = 10.0;


Route::Route(string const & router, vector<m2::PointD> const & points, string const & name)
  : m_router(router), m_poly(points), m_name(name)
{
  Update();
}

void Route::Swap(Route & rhs)
{
  m_router.swap(rhs.m_router);
  m_poly.Swap(rhs.m_poly);
  m_name.swap(rhs.m_name);
  m_segDistance.swap(rhs.m_segDistance);
  m_segProj.swap(rhs.m_segProj);
  swap(m_current, rhs.m_current);
  swap(m_currentTime, rhs.m_currentTime);
  swap(m_turns, rhs.m_turns);
}

void Route::SetTurnInstructions(TurnsT & v)
{
  swap(m_turns, v);
}

double Route::GetDistance() const
{
  ASSERT(!m_segDistance.empty(), ());
  return m_segDistance.back();
}

double Route::GetCurrentDistanceFromBegin() const
{
  ASSERT(m_current.IsValid(), ());

  return ((m_current.m_ind > 0 ? m_segDistance[m_current.m_ind - 1] : 0.0) +
          MercatorBounds::DistanceOnEarth(m_poly.GetPoint(m_current.m_ind), m_current.m_pt));
}

double Route::GetCurrentDistanceToEnd() const
{
  ASSERT(m_current.IsValid(), ());

  return (m_segDistance.back() - m_segDistance[m_current.m_ind] +
          MercatorBounds::DistanceOnEarth(m_current.m_pt, m_poly.GetPoint(m_current.m_ind + 1)));
}

bool Route::MoveIterator(location::GpsInfo const & info) const
{
  double predictDistance = -1.0;
  if (m_currentTime > 0.0 && info.HasSpeed())
  {
    /// @todo Need to distinguish GPS and WiFi locations.
    /// They may have different time metrics in case of incorrect system time on a device.
    double const deltaT = info.m_timestamp - m_currentTime;
    if (deltaT > 0.0 && deltaT < LOCATION_TIME_THRESHOLD)
      predictDistance = info.m_speed * deltaT;
  }

  m2::RectD const rect = MercatorBounds::MetresToXY(
        info.m_longitude, info.m_latitude,
        max(ON_ROAD_TOLERANCE_M, info.m_horizontalAccuracy));

  IterT const res = FindProjection(rect, predictDistance);
  if (res.IsValid())
  {
    m_current = res;
    m_currentTime = info.m_timestamp;
    return true;
  }
  else
    return false;
}

double Route::GetCurrentSqDistance(m2::PointD const & pt) const
{
  ASSERT(m_current.IsValid(), ());
  return pt.SquareLength(m_current.m_pt);
}

bool Route::IsCurrentOnEnd() const
{
  return (GetCurrentDistanceToEnd() < ON_END_TOLERANCE_M);
}

Route::IterT Route::FindProjection(m2::RectD const & posRect, double predictDistance) const
{
  ASSERT(m_current.IsValid(), ());
  ASSERT_LESS(m_current.m_ind, m_poly.GetSize() - 1, ());

  IterT res;
  if (predictDistance >= 0.0)
  {
    res = GetClosestProjection(posRect, [&] (IterT const & it)
    {
      return fabs(GetDistanceOnPolyline(m_current, it) - predictDistance);
    });
  }
  else
  {
    m2::PointD const currPos = posRect.Center();
    res = GetClosestProjection(posRect, [&] (IterT const & it)
    {
      return MercatorBounds::DistanceOnEarth(it.m_pt, currPos);
    });
  }

  return res;
}

double Route::GetDistanceOnPolyline(IterT const & it1, IterT const & it2) const
{
  ASSERT(it1.IsValid() && it2.IsValid(), ());
  ASSERT_LESS_OR_EQUAL(it1.m_ind, it2.m_ind, ());
  ASSERT_LESS(it1.m_ind, m_poly.GetSize(), ());
  ASSERT_LESS(it2.m_ind, m_poly.GetSize(), ());

  if (it1.m_ind == it2.m_ind)
    return MercatorBounds::DistanceOnEarth(it1.m_pt, it2.m_pt);

  return (MercatorBounds::DistanceOnEarth(it1.m_pt, m_poly.GetPoint(it1.m_ind + 1)) +
          m_segDistance[it2.m_ind - 1] - m_segDistance[it1.m_ind] +
          MercatorBounds::DistanceOnEarth(m_poly.GetPoint(it2.m_ind), it2.m_pt));
}

void Route::Update()
{
  size_t n = m_poly.GetSize();
  ASSERT_GREATER(n, 1, ());
  --n;

  m_segDistance.resize(n);
  m_segProj.resize(n);

  double dist = 0.0;
  for (size_t i = 0; i < n; ++i)
  {
    m2::PointD const & p1 = m_poly.GetPoint(i);
    m2::PointD const & p2 = m_poly.GetPoint(i + 1);

    dist += ms::DistanceOnEarth(MercatorBounds::YToLat(p1.y),
                                MercatorBounds::XToLon(p1.x),
                                MercatorBounds::YToLat(p2.y),
                                MercatorBounds::XToLon(p2.x));

    m_segDistance[i] = dist;
    m_segProj[i].SetBounds(p1, p2);
  }

  m_current = IterT(m_poly.Front(), 0);
  m_currentTime = 0.0;
}

template <class DistanceF>
Route::IterT Route::GetClosestProjection(m2::RectD const & posRect, DistanceF const & distFn) const
{
  IterT res;
  double minDist = numeric_limits<double>::max();

  m2::PointD const currPos = posRect.Center();
  size_t const count = m_poly.GetSize() - 1;
  for (size_t i = m_current.m_ind; i < count; ++i)
  {
    m2::PointD const pt = m_segProj[i](currPos);

    if (!posRect.IsPointInside(pt))
      continue;

    IterT it(pt, i);
    double const dp = distFn(it);
    if (dp < minDist)
    {
      res = it;
      minDist = dp;
    }
  }

  return res;
}

string DebugPrint(Route const & r)
{
  return DebugPrint(r.m_poly);
}

} // namespace routing
