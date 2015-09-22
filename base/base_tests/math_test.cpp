#include "../../testing/testing.hpp"
#include "../math.hpp"
#include "../../std/limits.hpp"
#include "../../base/start_mem_debug.hpp"

UNIT_TEST(id)
{
    TEST_EQUAL(my::id(true), true, ());
    TEST_EQUAL(my::id(1), 1.0, ());
    TEST_EQUAL(my::id(1.0), 1, ());
}

UNIT_TEST(sq)
{
    TEST_EQUAL(my::sq(-1.5), 2.25, ());
    TEST_EQUAL(my::sq(3), 9, ());
}

UNIT_TEST(SizeAligned)
{
    TEST_EQUAL(my::SizeAligned(0, 1), size_t(0), ());
    TEST_EQUAL(my::SizeAligned(1, 1), size_t(1), ());
    TEST_EQUAL(my::SizeAligned(2, 1), size_t(2), ());
    TEST_EQUAL(my::SizeAligned(3, 1), size_t(3), ());

    TEST_EQUAL(my::SizeAligned(0, 8), size_t(0), ());
    TEST_EQUAL(my::SizeAligned(1, 8), size_t(8), ());
    TEST_EQUAL(my::SizeAligned(2, 8), size_t(8), ());
    TEST_EQUAL(my::SizeAligned(7, 8), size_t(8), ());
    TEST_EQUAL(my::SizeAligned(8, 8), size_t(8), ());
    TEST_EQUAL(my::SizeAligned(9, 8), size_t(16), ());
    TEST_EQUAL(my::SizeAligned(15, 8), size_t(16), ());
    TEST_EQUAL(my::SizeAligned(16, 8), size_t(16), ());
    TEST_EQUAL(my::SizeAligned(17, 8), size_t(24), ());
}

UNIT_TEST(PowUInt)
{
  TEST_EQUAL(my::PowUint(3, 10), 59049, ());
}

UNIT_TEST(AlmostEqual_Smoke)
{
  TEST_ALMOST_EQUAL(3.0, 3.0, ());
  TEST_ALMOST_EQUAL(+0.0, -0.0, ());

  double const eps = numeric_limits<double>::epsilon();
  double const dmax = numeric_limits<double>::max();

  TEST_ALMOST_EQUAL(1.0 + eps, 1.0, ());
  TEST_ALMOST_EQUAL(1.0 - eps, 1.0, ());
  TEST_ALMOST_EQUAL(1.0 - eps, 1.0 + eps, ());

  TEST_ALMOST_EQUAL(dmax, dmax, ());
  TEST_ALMOST_EQUAL(-dmax, -dmax, ());
  TEST_ALMOST_EQUAL(dmax/2.0, dmax/2.0, ());
  TEST_ALMOST_EQUAL(1.0/dmax, 1.0/dmax, ());
  TEST_ALMOST_EQUAL(-1.0/dmax, -1.0/dmax, ());

  TEST(!my::AlmostEqual(1.0, -1.0), ());
  TEST(!my::AlmostEqual(2.0, -2.0), ());
  TEST(!my::AlmostEqual(dmax, -dmax), ());
}

namespace
{

// Returns the next representable floating point value without using conversion to integer.
template <typename FloatT> FloatT NextFloat(FloatT const x, int dir = 1)
{
  FloatT d = dir * numeric_limits<FloatT>::denorm_min();
  while (true)
  {
    FloatT y = x;
    y += d;
    if (x != y)
      return y;
    d *= 2;
  }
}

template <typename FloatT> void TestMaxULPs()
{
  for (unsigned int logMaxULPs = 0; logMaxULPs <= 8; ++logMaxULPs)
  {
    unsigned int const maxULPs = (logMaxULPs == 0 ? 0 : ((1 << logMaxULPs) - 1));
    for (int base = -1; base <= 1; ++base)
    {
      for (int dir = -1; dir <= 1; dir += 2)
      {
        FloatT const x = base;
        FloatT y = x;
        for (unsigned int i = 0; i <= maxULPs; ++i)
        {
          TEST(my::AlmostEqual(x, y, maxULPs), (x, y, maxULPs, x - y, dir));
          FloatT const nextY = NextFloat(y, dir);
          TEST_NOT_EQUAL(y, nextY, (i, base, dir));
          y = nextY;
        }
        TEST(!my::AlmostEqual(x, y, maxULPs), (x, y, maxULPs, x - y));
      }
    }
  }
}

}

UNIT_TEST(AlmostEqual_MaxULPs_double)
{
  TestMaxULPs<double>();
}

UNIT_TEST(AlmostEqual_MaxULPs_float)
{
  TestMaxULPs<float>();
}

UNIT_TEST(TEST_FLOAT_DOUBLE_EQUAL_macros)
{
  float const fx = 3;
  float const fy = NextFloat(NextFloat(NextFloat(fx)));
  TEST_ALMOST_EQUAL(fx, fy, ());
  TEST_NOT_ALMOST_EQUAL(fx, 2.0f, ());

  double const dx = 3;
  double const dy = NextFloat(NextFloat(NextFloat(dx)));
  TEST_ALMOST_EQUAL(dx, dy, ());
  TEST_NOT_ALMOST_EQUAL(dx, 2.0, ());
}

UNIT_TEST(IsIntersect_Intervals)
{
  TEST(my::IsIntersect(0, 100, 100, 200), ());
  TEST(!my::IsIntersect(0, 100, 150, 200), ());
  TEST(my::IsIntersect(0, 100, 50, 150), ());
  TEST(my::IsIntersect(0, 100, 50, 80), ());
  TEST(my::IsIntersect(0, 100, -50, 50), ());
  TEST(my::IsIntersect(0, 100, -50, 0), ());
  TEST(!my::IsIntersect(0, 100, -50, -20), ());
}

#include "../../base/stop_mem_debug.hpp"
