#pragma once
#include "cell_id.hpp"
#include "feature_data.hpp"

#include "../geometry/point2d.hpp"
#include "../geometry/rect2d.hpp"

#include "../base/buffer_vector.hpp"

#include "../std/string.hpp"
#include "../std/shared_ptr.hpp"


namespace feature
{
  class LoaderBase;
  class LoaderCurrent;
}

namespace old_101 { namespace feature
{
  class LoaderImpl;
}}


/// Base feature class for storing common data (without geometry).
class FeatureBase
{
  static const int m_maxTypesCount = feature::max_types_count;

public:

  typedef char const * BufferT;

  void Deserialize(feature::LoaderBase * pLoader, BufferT buffer);

  /// @name Parse functions. Do simple dispatching to m_pLoader.
  //@{
  void ParseTypes() const;
  void ParseCommon() const;
  //@}

  feature::EGeomType GetFeatureType() const;

  inline uint8_t GetTypesCount() const
  {
    return ((Header() & feature::HEADER_TYPE_MASK) + 1);
  }

  inline int8_t GetLayer() const
  {
    if (!(Header() & feature::HEADER_HAS_LAYER))
      return 0;

    ParseCommon();
    return m_Params.layer;
  }

  inline bool HasName() const
  {
    return (Header() & feature::HEADER_HAS_NAME) != 0;
  }

  class GetNamesFn
  {
  public:
    string m_names[64];
    char m_langs[64];
    size_t m_size;

    GetNamesFn() : m_size(0) {}
    bool operator() (char lang, string const & name)
    {
      m_names[m_size++] = name;
      m_langs[m_size] = lang;
      return true;
    }
  };

  template <class T>
  inline bool ForEachNameRef(T & functor) const
  {
    if (!HasName())
      return false;

    ParseCommon();
    m_Params.name.ForEachRef(functor);
    return true;
  }

  inline m2::RectD GetLimitRect() const
  {
    ASSERT ( m_LimitRect != m2::RectD::GetEmptyRect(), () );
    return m_LimitRect ;
  }

  class GetTypesFn
  {
  public:
    uint32_t m_types[m_maxTypesCount];
    size_t m_size;

    GetTypesFn() : m_size(0) {}
    void operator() (uint32_t t)
    {
      m_types[m_size++] = t;
    }
  };

  template <typename FunctorT>
  void ForEachTypeRef(FunctorT & f) const
  {
    ParseTypes();

    uint32_t const count = GetTypesCount();
    for (size_t i = 0; i < count; ++i)
      f(m_Types[i]);
  }

protected:
  /// @name Need for FeatureBuilder.
  //@{
  friend class FeatureBuilder1;
  inline void SetHeader(uint8_t h) { m_Header = h; }
  //@}

  string DebugString() const;

  inline uint8_t Header() const { return m_Header; }

protected:
  shared_ptr<feature::LoaderBase> m_pLoader;

  uint8_t m_Header;

  mutable uint32_t m_Types[m_maxTypesCount];

  mutable FeatureParamsBase m_Params;

  mutable m2::PointD m_Center;

  mutable m2::RectD m_LimitRect;

  mutable bool m_bTypesParsed, m_bCommonParsed;

  friend class feature::LoaderCurrent;
  friend class old_101::feature::LoaderImpl;
};

/// Working feature class with geometry.
class FeatureType : public FeatureBase
{
  typedef FeatureBase base_type;

public:
  void Deserialize(feature::LoaderBase * pLoader, BufferT buffer);

  /// @name Parse functions. Do simple dispatching to m_pLoader.
  //@{
  void ParseHeader2() const;
  uint32_t ParseGeometry(int scale) const;
  uint32_t ParseTriangles(int scale) const;
  //@}

  /// @name Geometry.
  //@{
  m2::RectD GetLimitRect(int scale) const;

  bool IsEmptyGeometry(int scale) const;

  template <typename FunctorT>
  void ForEachPointRef(FunctorT & f, int scale) const
  {
    ParseGeometry(scale);

    if (m_Points.empty())
    {
      // it's a point feature
      if (GetFeatureType() == feature::GEOM_POINT)
        f(CoordPointT(m_Center.x, m_Center.y));
    }
    else
    {
      for (size_t i = 0; i < m_Points.size(); ++i)
        f(CoordPointT(m_Points[i].x, m_Points[i].y));
    }
  }

  template <typename FunctorT>
  void ForEachPoint(FunctorT f, int scale) const
  {
    ForEachPointRef(f, scale);
  }

  template <typename FunctorT>
  void ForEachTriangleRef(FunctorT & f, int scale) const
  {
    ParseTriangles(scale);

    for (size_t i = 0; i < m_Triangles.size();)
    {
      f(m_Triangles[i], m_Triangles[i+1], m_Triangles[i+2]);
      i += 3;
    }
  }

  template <typename FunctorT>
  void ForEachTriangleExRef(FunctorT & f, int scale) const
  {
    f.StartPrimitive(m_Triangles.size());
    ForEachTriangleRef(f, scale);
    f.EndPrimitive();
  }
  //@}

  /// For test cases only.
  string DebugString(int scale) const;

  /// @param priorities optional array of languages priorities
  /// if NULL, default (0) lang will be used
  string GetPreferredDrawableName(int8_t const * priorities = NULL) const;

  uint32_t GetPopulation() const;
  double GetPopulationDrawRank() const;
  uint8_t GetSearchRank() const;

  inline string GetRoadNumber() const { return m_Params.ref; }

  /// @name Statistic functions.
  //@{
  inline void ParseBeforeStatistic() const
  {
    ParseHeader2();
  }

  struct inner_geom_stat_t
  {
    uint32_t m_Points, m_Strips, m_Size;

    void MakeZero()
    {
      m_Points = m_Strips = m_Size = 0;
    }
  };

  inner_geom_stat_t GetInnerStatistic() const { return m_InnerStats; }

  struct geom_stat_t
  {
    uint32_t m_size, m_count;

    geom_stat_t(uint32_t sz, size_t count)
      : m_size(sz), m_count(static_cast<uint32_t>(count))
    {
    }

    geom_stat_t() : m_size(0), m_count(0) {}
  };

  geom_stat_t GetGeometrySize(int scale) const;
  geom_stat_t GetTrianglesSize(int scale) const;
  //@}

private:
  void ParseAll(int scale) const;

  // For better result this value should be greater than 17
  // (number of points in inner triangle-strips).
  static const size_t static_buffer = 32;

  typedef buffer_vector<m2::PointD, static_buffer> points_t;
  mutable points_t m_Points, m_Triangles;

  mutable bool m_bHeader2Parsed, m_bPointsParsed, m_bTrianglesParsed;

  mutable inner_geom_stat_t m_InnerStats;

  friend class feature::LoaderCurrent;
  friend class old_101::feature::LoaderImpl;
};

namespace feature
{
  template <class TCont>
  void CalcRect(TCont const & points, m2::RectD & rect)
  {
    for (size_t i = 0; i < points.size(); ++i)
      rect.Add(points[i]);
  }
}
