#pragma once

#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/route_shape.hpp"

#include "drape/pointers.hpp"
#include "drape/texture_manager.hpp"

#include "geometry/polyline2d.hpp"

#include "std/function.hpp"
#include "std/unordered_map.hpp"

namespace df
{

class RouteBuilder
{
public:
  using TFlushRouteFn = function<void(drape_ptr<RouteData> &&)>;
  using TFlushRouteSignFn = function<void(drape_ptr<RouteSignData> &&)>;
  using TFlushRouteArrowsFn = function<void(drape_ptr<RouteArrowsData> &&)>;

  RouteBuilder(TFlushRouteFn const & flushRouteFn,
               TFlushRouteSignFn const & flushRouteSignFn,
               TFlushRouteArrowsFn const & flushRouteArrowsFn);

  void Build(m2::PolylineD const & routePolyline, vector<double> const & turns,
             df::ColorConstant color, df::RoutePattern const & pattern,
             ref_ptr<dp::TextureManager> textures);

  void BuildArrows(int routeIndex, vector<ArrowBorders> const & borders,
                   ref_ptr<dp::TextureManager> textures);

  void BuildSign(m2::PointD const & pos, bool isStart, bool isValid,
                 ref_ptr<dp::TextureManager> textures);

  void ClearRouteCache();

private:
  TFlushRouteFn m_flushRouteFn;
  TFlushRouteSignFn m_flushRouteSignFn;
  TFlushRouteArrowsFn m_flushRouteArrowsFn;

  int m_routeIndex = 0;
  unordered_map<int, m2::PolylineD> m_routeCache;
};

} // namespace df
