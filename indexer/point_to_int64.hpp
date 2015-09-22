#pragma once

#include "../geometry/cellid.hpp"
#include "../geometry/rect2d.hpp"

#include "../std/utility.hpp"


#define POINT_COORD_BITS 30


typedef double CoordT;
typedef pair<CoordT, CoordT> CoordPointT;

typedef m2::CellId<19> RectId;

m2::PointU PointD2PointU(CoordT x, CoordT y, uint32_t coordBits);
inline m2::PointU PointD2PointU(m2::PointD const & pt, uint32_t coordBits)
{
  return PointD2PointU(pt.x, pt.y, coordBits);
}

CoordPointT PointU2PointD(m2::PointU const & p, uint32_t coordBits);

int64_t PointToInt64(CoordT x, CoordT y, uint32_t coordBits);
inline int64_t PointToInt64(CoordPointT const & pt, uint32_t coordBits)
{
  return PointToInt64(pt.first, pt.second, coordBits);
}
inline int64_t PointToInt64(m2::PointD const & pt, uint32_t coordBits)
{
  return PointToInt64(pt.x, pt.y, coordBits);
}

CoordPointT Int64ToPoint(int64_t v, uint32_t coordBits);

pair<int64_t, int64_t> RectToInt64(m2::RectD const & r, uint32_t coordBits);
m2::RectD Int64ToRect(pair<int64_t, int64_t> const & p, uint32_t coordBits);
