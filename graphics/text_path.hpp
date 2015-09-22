#pragma once

#include "../base/matrix.hpp"
#include "../base/buffer_vector.hpp"

#include "glyph_cache.hpp"
#include "path_view.hpp"

namespace graphics
{
  class TextPath
  {
  private:

    buffer_vector<m2::PointD, 8> m_arr;
    PathView m_pv;

    double m_fullLength;
    double m_pathOffset;

    void checkReverse();

  public:

    TextPath();
    TextPath(TextPath const & src, math::Matrix<double, 3, 3> const & m);
    TextPath(m2::PointD const * arr, size_t sz, double fullLength, double pathOffset);

    size_t size() const;

    m2::PointD get(size_t i) const;
    m2::PointD operator[](size_t i) const;

    double fullLength() const;
    double pathOffset() const;

    void setIsReverse(bool flag);
    bool isReverse() const;

    PathPoint const offsetPoint(PathPoint const & pp, double offset);
    PivotPoint findPivotPoint(PathPoint const & pp, GlyphMetrics const & sym, double kern);

    PathPoint const front() const;
  };
}
