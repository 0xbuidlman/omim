#pragma once

#include "../base/assert.hpp"
#include "../base/base.hpp"
#include "../base/math.hpp"
#include "../base/matrix.hpp"
#include "../std/cmath.hpp"
#include "../std/sstream.hpp"
#include "../std/typeinfo.hpp"

#include "../base/start_mem_debug.hpp"

namespace m2
{
  template <typename T>
  class Point
  {
  public:
    typedef T value_type;

    T x, y;

    Point() {}
    Point(T x_, T y_) : x(x_), y(y_) {}
    template <typename U>
    Point(Point<U> const & u) : x(u.x), y(u.y) {}

    bool EqualDxDy(m2::Point<T> const & p, T eps) const
    {
      return ((fabs(x - p.x) < eps) && (fabs(y - p.y) < eps));
    }

    double SquareLength(m2::Point<T> const & p) const
    {
      return math::sqr(x - p.x) + math::sqr(y - p.y);
    }

    double Length(m2::Point<T> const & p) const
    {
      return sqrt(SquareLength(p));
    }

    m2::Point<T> Move(T len, T ang) const
    {
      return m2::Point<T>(x + len * cos(ang), y + len * sin(ang));
    }

    Point<T> const & operator-=(Point<T> const & a)
    {
      x -= a.x;
      y -= a.y;
      return *this;
    }

    Point<T> const & operator+=(Point<T> const & a)
    {
      x += a.x;
      y += a.y;
      return *this;
    }

    template <typename U>
    Point<T> const & operator*=(U const & k)
    {
      x = static_cast<T>(x * k);
      y = static_cast<T>(y * k);
      return *this;
    }

    template <typename U>
    Point<T> const & operator=(Point<U> const & a)
    {
      x = static_cast<T>(a.x);
      y = static_cast<T>(a.y);
      return *this;
    }

    bool operator == (m2::Point<T> const & p) const
    {
      return x == p.x && y == p.y;
    }
    bool operator != (m2::Point<T> const & p) const
    {
      return !(*this == p);
    }
    m2::Point<T> operator + (m2::Point<T> const & pt) const
    {
      return m2::Point<T>(x + pt.x, y + pt.y);
    }
    m2::Point<T> operator - (m2::Point<T> const & pt) const
    {
      return m2::Point<T>(x - pt.x, y - pt.y);
    }
    m2::Point<T> operator -() const
    {
      return m2::Point<T>(-x, -y);
    }

    m2::Point<T> operator * (T scale) const
    {
      return m2::Point<T>(x * scale, y * scale);
    }

    m2::Point<T> const operator * (math::Matrix<T, 3, 3> const & m) const
    {
      m2::Point<T> res;
      res.x = x * m(0, 0) + y * m(1, 0) + m(2, 0);
      res.y = x * m(0, 1) + y * m(1, 1) + m(2, 1);
      return res;
    }

    m2::Point<T> operator / (T scale) const
    {
      return m2::Point<T>(x / scale, y / scale);
    }

    m2::Point<T> const & operator *= (math::Matrix<T, 3, 3> const & m)
    {
      T tempX = x;
      x = tempX * m(0, 0) + y * m(1, 0) + m(2, 0);
      y = tempX * m(0, 1) + y * m(1, 1) + m(2, 1);
      return *this;
    }

    void Rotate(double angle)
    {
      T cosAngle = cos(angle);
      T sinAngle = sin(angle);
      T oldX = x;
      x = cosAngle * oldX - sinAngle * y;
      y = sinAngle * oldX + cosAngle * y;
    }

    void Transform(m2::Point<T> const & org,
                   m2::Point<T> const & dx, m2::Point<T> const & dy)
    {
      T oldX = x;
      x = org.x + oldX * dx.x + y * dy.x;
      y = org.y + oldX * dx.y + y * dy.y;
    }
  };

  template <typename T>
  inline Point<T> const operator- (Point<T> const & a, Point<T> const & b)
  {
    return Point<T>(a.x - b.x, a.y - b.y);
  }

  template <typename T>
  inline Point<T> const operator+ (Point<T> const & a, Point<T> const & b)
  {
    return Point<T>(a.x + b.x, a.y + b.y);
  }

  // Dot product of a and b, equals to |a|*|b|*cos(angle_between_a_and_b).
  template <typename T>
  T const DotProduct(Point<T> const & a, Point<T> const & b)
  {
    return a.x * b.x + a.y * b.y;
  }

  // Value of cross product of a and b, equals to |a|*|b|*sin(angle_between_a_and_b).
  template <typename T>
  T const CrossProduct(Point<T> const & a, Point<T> const & b)
  {
    return a.x * b.y - a.y * b.x;
  }

  template <typename T>
      Point<T> const Rotate(Point<T> const & pt, T a)
  {
    Point<T> res(pt);
    res.Rotate(a);
    return res;
  }

  template <typename T, typename U>
      Point<T> const Shift(Point<T> const & pt, U const & dx, U const & dy)
  {
    return Point<T>(pt.x + dx, pt.y + dy);
  }

  template <typename T, typename U>
      Point<T> const Shift(Point<T> const & pt, Point<U> const & offset)
  {
    return Shift(pt, offset.x, offset.y);
  }

  template <typename T>
  bool IsPointStrictlyInsideTriangle(Point<T> const & p,
                                     Point<T> const & a, Point<T> const & b, Point<T> const & c)
  {
    T const cpab = CrossProduct(b - a, p - a);
    T const cpbc = CrossProduct(c - b, p - b);
    T const cpca = CrossProduct(a - c, p - c);
    return (cpab < 0 && cpbc < 0 && cpca < 0) || (cpab > 0 && cpbc > 0 && cpca > 0);
  }

  template <typename T>
  bool SegmentsIntersect(Point<T> const & a, Point<T> const & b,
                         Point<T> const & c, Point<T> const & d)
  {
    return
        max(a.x, b.x) >= min(c.x, d.x) &&
        min(a.x, b.x) <= max(c.x, d.x) &&
        max(a.y, b.y) >= min(c.y, d.y) &&
        min(a.y, b.y) <= max(c.y, d.y) &&
        CrossProduct(c - a, b - a) * CrossProduct(d - a, b - a) <= 0 &&
        CrossProduct(a - c, d - c) * CrossProduct(b - c, d - c) <= 0;
  }

  template <typename T> string debug_print(m2::Point<T> const & p)
  {
    ostringstream out;
    out.precision(15);
    out << "m2::Point<" << typeid(T).name() << ">(" << p.x << ", " << p.y << ")";
    return out.str();
  }

  template <typename T> bool AlmostEqual(m2::Point<T> const & a, m2::Point<T> const & b)
  {
    return my::AlmostEqual(a.x, b.x) && my::AlmostEqual(a.y, b.y);
  }

  typedef Point<float> PointF;
  typedef Point<double> PointD;
  typedef Point<uint32_t> PointU;
  typedef Point<uint64_t> PointU64;
  typedef Point<int32_t> PointI;
  typedef Point<int64_t> PointI64;
}

#include "../base/stop_mem_debug.hpp"
