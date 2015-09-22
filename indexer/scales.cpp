#include "scales.hpp"
#include "mercator.hpp"

#include "../base/math.hpp"

#include "../std/algorithm.hpp"

#include "../base/start_mem_debug.hpp"

namespace scales
{
  /// @name This parameters should be tuned.
  //@{
  static const int initial_level = 1;

  double GetM2PFactor(int level)
  {
    int const base_scale = 14;
    int const factor = 1 << my::Abs(level - base_scale);

    if (level < base_scale)
      return 1 / double(factor);
    else
      return factor;
  }
  //@}

  double GetScaleLevelD(double ratio)
  {
    double const level = min(static_cast<double>(GetUpperScale()), log(ratio) / log(2.0) + initial_level);
    return (level < 0.0 ? 0.0 : level);
  }

  double GetScaleLevelD(m2::RectD const & r)
  {
    // TODO: fix scale factors for mercator projection
    double const dx = (MercatorBounds::maxX - MercatorBounds::minX) / r.SizeX();
    double const dy = (MercatorBounds::maxY - MercatorBounds::minY) / r.SizeY();

    // get the average ratio
    return GetScaleLevelD((dx + dy) / 2.0);
  }

  int GetScaleLevel(double ratio)
  {
    return my::rounds(GetScaleLevelD(ratio));
  }

  int GetScaleLevel(m2::RectD const & r)
  {
    return my::rounds(GetScaleLevelD(r));
  }

  double GetRationForLevel(int level)
  {
    if (level < initial_level) level = initial_level;
    return pow(2.0, level - initial_level);
  }

  m2::RectD GetRectForLevel(int level, m2::PointD const & center, double X2YRatio)
  {
    double const dy = 2.0*GetRationForLevel(level) / (1.0 + X2YRatio);
    double const dx = X2YRatio * dy;
    ASSERT_GREATER ( dy, 0.0, () );
    ASSERT_GREATER ( dx, 0.0, () );

    double const xL = (MercatorBounds::maxX - MercatorBounds::minX) / (2.0*dx);
    double const yL = (MercatorBounds::maxY - MercatorBounds::minY) / (2.0*dy);
    ASSERT_GREATER ( xL, 0.0, () );
    ASSERT_GREATER ( yL, 0.0, () );

    return m2::RectD(center.x - xL, center.y - yL, center.x + xL, center.y + yL);
  }

  namespace
  {
    double GetEpsilonImpl(int level, int logEps)
    {
      return (MercatorBounds::maxX - MercatorBounds::minX) / pow(2.0, double(level + logEps - initial_level));
    }
  }

  double GetEpsilonForLevel(int level)
  {
    return GetEpsilonImpl(level, 6);
  }

  double GetEpsilonForSimplify(int level)
  {
    return GetEpsilonImpl(level, 9);
  }

  bool IsGoodForLevel(int level, m2::RectD const & r)
  {
    // assume that feature is always visible in upper scale
    return (level == GetUpperScale() || max(r.SizeX(), r.SizeY()) > GetEpsilonForLevel(level));
  }
}
