#include "../../testing/testing.hpp"

#include "../../platform/platform.hpp"

#include "../../map/feature_vec_model.hpp"

#include "../../indexer/data_header.hpp"
#include "../../indexer/scales.hpp"
#include "../../indexer/feature_visibility.hpp"
#include "../../indexer/feature_processor.hpp"
#include "../../indexer/classificator.hpp"

#include "../../geometry/rect_intersect.hpp"
#include "../../geometry/robust_orientation.hpp"

#include "../../base/logging.hpp"

#include "../../std/string.hpp"
#include "../../std/algorithm.hpp"


namespace
{

typedef vector<uint32_t> feature_cont_t;

bool IsDrawable(FeatureType const & f, int scale)
{
  // Feature that doesn't have any geometry for m_scale returns empty DebugString().
  return (!f.IsEmptyGeometry(scale) && feature::IsDrawableForIndex(f, scale));
}

class AccumulatorBase
{
  feature_cont_t & m_cont;

protected:
  int m_scale;

  bool is_drawable(FeatureType const & f) const
  {
    // Looks strange, but it checks consistency.
    TEST_EQUAL(f.DebugString(m_scale), f.DebugString(m_scale), ());

    return IsDrawable(f, m_scale);
  }

  void add(FeatureType const & f) const
  {
    TEST(f.GetID().IsValid(), ());
    m_cont.push_back(f.GetID().m_offset);
  }

  void add(FeatureType const &, uint32_t offset) const
  {
    m_cont.push_back(offset);
  }

public:
  AccumulatorBase(int scale, feature_cont_t & cont)
    : m_cont(cont), m_scale(scale)
  {
  }

  void operator() (FeatureType const & f) const
  {
    TEST(is_drawable(f), ());
    add(f);
  }
};

class IntersectCheck
{
  m2::RectD m_rect;

  m2::PointD m_prev;
  bool m_isPrev, m_intersect;

public:
  IntersectCheck(m2::RectD const & r)
    : m_rect(r), m_isPrev(false), m_intersect(false)
  {
  }

  void TestPoint(m2::PointD const & p)
  {
    m_intersect = m_rect.IsPointInside(p);
  }

  void operator() (m2::PointD const & pt)
  {
    if (m_intersect) return;

    if (m_isPrev)
    {
      m2::PointD d1 = m_prev;
      m2::PointD d2 = pt;
      m_intersect = m2::Intersect(m_rect, d1, d2);
    }
    else
      m_isPrev = true;

    m_prev = pt;
  }

  void operator() (m2::PointD const & p1, m2::PointD const & p2, m2::PointD const & p3)
  {
    if (m_intersect) return;

    m2::PointD arrP[] = { p1, p2, p3 };

    // make right-oriented triangle
    if (m2::robust::OrientedS(arrP[0], arrP[1], arrP[2]) < 0.0)
      swap(arrP[1], arrP[2]);

    bool isInside = true;
    m2::PointD const pt = m_rect.LeftTop();

    for (size_t i = 0; i < 3; ++i)
    {
      // intersect edge with rect
      m_intersect = m2::Intersect(m_rect, arrP[i], arrP[(i + 1) % 3]);
      if (m_intersect)
        break;

      if (isInside)
      {
        // or check if rect inside triangle (any point of rect)
        double const s = m2::robust::OrientedS(arrP[i], arrP[(i + 1) % 3], pt);
        isInside = (s >= 0.0);
      }
    }

    m_intersect = m_intersect || isInside;
  }

  bool IsIntersect() const { return m_intersect; }
};

class AccumulatorEtalon : public AccumulatorBase
{
  typedef AccumulatorBase base_type;

  m2::RectD m_rect;

  bool is_intersect(FeatureType const & f) const
  {
    IntersectCheck check(m_rect);

    using namespace feature;
    switch (f.GetFeatureType())
    {
    case GEOM_POINT: check.TestPoint(f.GetCenter()); break;
    case GEOM_LINE: f.ForEachPointRef(check, m_scale); break;
    case GEOM_AREA: f.ForEachTriangleRef(check, m_scale); break;
    default:
      CHECK ( false, () );
    }

    return check.IsIntersect();
  }

public:
  AccumulatorEtalon(m2::RectD const & r, int scale, feature_cont_t & cont)
    : base_type(scale, cont), m_rect(r)
  {
  }

  void operator() (FeatureType const & f, uint64_t offset) const
  {
    if (is_drawable(f) && is_intersect(f))
      add(f, offset);
  }
};

// Use this comparator for "sort" and "compare_sequence".
struct FeatureIDCmp
{
  int compare(uint32_t r1, uint32_t r2) const
  {
    if (r1 < r2)
      return -1;
    if (r2 < r1)
      return 1;
    return 0;
  }
  bool operator() (uint32_t r1, uint32_t r2) const
  {
    return (compare(r1, r2) == -1);
  }
};

/// Check that "test" contains all elements from "etalon".
template <class TCont, class TCompare>
bool compare_sequence(TCont const & etalon, TCont const & test, TCompare comp, size_t & errInd)
{
  auto i1 = etalon.begin();
  auto i2 = test.begin();
  while (i1 != etalon.end() && i2 != test.end())
  {
    switch (comp.compare(*i1, *i2))
    {
    case 0:   // equal
      ++i1;
      ++i2;
      break;
    case -1:  // present in etalon, but missing in test - error
      {
        errInd = distance(etalon.begin(), i1);
        return false;
      }
    case 1:   // present in test, but missing in etalon - actually it may be ok
      ++i2;
      break;
    }
  }

  if (i1 != etalon.end())
  {
    errInd = distance(etalon.begin(), i1);
    return false;
  }

  return true;
}

class FindOffset
{
  int m_level;
  uint32_t m_offset;

public:
  FindOffset(int level, uint32_t offset)
    : m_level(level), m_offset(offset)
  {}

  void operator() (FeatureType const & f, uint64_t offset)
  {
    if (offset == m_offset)
    {
      TEST(IsDrawable(f, m_level), ());

      LOG(LINFO, ("Feature offset:", offset));
      LOG(LINFO, ("Feature:", f));
    }
  }
};

void RunTest(string const & file)
{
  model::FeaturesFetcher src1;
  src1.InitClassificator();

  src1.RegisterMap(file);

  vector<m2::RectD> rects;
  rects.push_back(src1.GetWorldRect());

  ModelReaderPtr reader = GetPlatform().GetReader(file);

  while (!rects.empty())
  {
    m2::RectD const r = rects.back();
    rects.pop_back();

    int const scale = scales::GetScaleLevel(r);

    feature_cont_t v1, v2;
    {
      AccumulatorBase acc(scale, v1);
      src1.ForEachFeature(r, acc, scale);
      sort(v1.begin(), v1.end(), FeatureIDCmp());
    }
    {
      AccumulatorEtalon acc(r, scale, v2);
      feature::ForEachFromDat(reader, acc);
      sort(v2.begin(), v2.end(), FeatureIDCmp());
    }

    size_t const emptyInd = size_t(-1);
    size_t errInd = emptyInd;
    if (!compare_sequence(v2, v1, FeatureIDCmp(), errInd))
    {
      if (errInd != emptyInd)
      {
        FindOffset doFind(scale, v2[errInd]);
        feature::ForEachFromDat(reader, doFind);
      }

      TEST(false, ("Failed for rect:", r, "; Scale level:", scale, "; Etalon size:", v2.size(), "; Index size:", v1.size()));
    }

    if (!v2.empty() && (scale < scales::GetUpperScale()))
    {
      m2::RectD r1, r2;
      r.DivideByGreaterSize(r1, r2);
      rects.push_back(r1);
      rects.push_back(r2);
    }
  }
}

void RunTestForChoice(string const & fName)
{
  RunTest(fName + DATA_FILE_EXTENSION);
}

}

UNIT_TEST(ForEach_QueryResults)
{
  RunTestForChoice("minsk-pass");
  //RunTestForChoice("london-center");
}
