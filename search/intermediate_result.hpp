#pragma once
#include "result.hpp"

#include "../indexer/feature_data.hpp"


class FeatureType;
class CategoriesHolder;

namespace storage
{
  class CountryInfoGetter;
  struct CountryInfo;
}

namespace search
{
namespace impl
{

template <class T> bool LessRankT(T const & r1, T const & r2);
template <class T> bool LessViewportDistanceT(T const & r1, T const & r2);
template <class T> bool LessDistanceT(T const & r1, T const & r2);

/// First pass results class. Objects are creating during search in trie.
/// Works fast without feature loading and provide ranking.
class PreResult1
{
  friend class PreResult2;
  template <class T> friend bool LessRankT(T const & r1, T const & r2);
  template <class T> friend bool LessViewportDistanceT(T const & r1, T const & r2);
  template <class T> friend bool LessDistanceT(T const & r1, T const & r2);

  FeatureID m_id;
  m2::PointD m_center;
  double m_distance, m_distanceFromViewportCenter;
  uint8_t m_viewportDistance;
  uint8_t m_rank;
  int8_t m_viewportID;

  void CalcParams(m2::RectD const & viewport, m2::PointD const & pos);

public:
  PreResult1(FeatureID const & fID, uint8_t rank, m2::PointD const & center,
             m2::PointD const & pos, m2::RectD const & viewport, int8_t viewportID);
  PreResult1(m2::PointD const & center, m2::PointD const & pos, m2::RectD const & viewport);

  static bool LessRank(PreResult1 const & r1, PreResult1 const & r2);
  static bool LessDistance(PreResult1 const & r1, PreResult1 const & r2);
  static bool LessViewportDistance(PreResult1 const & r1, PreResult1 const & r2);
  static bool LessPointsForViewport(PreResult1 const & r1, PreResult1 const & r2);

  inline FeatureID GetID() const { return m_id; }
  inline m2::PointD GetCenter() const { return m_center; }
  inline uint8_t GetRank() const { return m_rank; }
  inline int8_t GetViewportID() const { return m_viewportID; }
};


/// Second result class. Objects are creating during reading of features.
/// Read and fill needed info for ranking and getting final results.
class PreResult2
{
  void CalcParams(m2::RectD const & viewport, m2::PointD const & pos);

public:
  enum ResultType
  {
    RESULT_LATLON,
    RESULT_FEATURE
  };

  // For RESULT_FEATURE.
  PreResult2(FeatureType const & f, PreResult1 const * p,
             m2::RectD const & viewport, m2::PointD const & pos,
             string const & displayName, string const & fileName);

  // For RESULT_LATLON.
  PreResult2(m2::RectD const & viewport, m2::PointD const & pos,
             double lat, double lon);

  /// @param[in]  pInfo   Need to get region for result.
  /// @param[in]  pCat    Categories need to display readable type string.
  /// @param[in]  pTypes  Set of preffered types that match input tokens by categories.
  /// @param[in]  lang    Current system language.
  Result GenerateFinalResult(storage::CountryInfoGetter const * pInfo,
                             CategoriesHolder const * pCat,
                             set<uint32_t> const * pTypes,
                             int8_t lang) const;

  Result GeneratePointResult(CategoriesHolder const * pCat,
                             set<uint32_t> const * pTypes,
                             int8_t lang) const;

  static bool LessRank(PreResult2 const & r1, PreResult2 const & r2);
  static bool LessDistance(PreResult2 const & r1, PreResult2 const & r2);
  static bool LessViewportDistance(PreResult2 const & r1, PreResult2 const & r2);

  /// Filter equal features for different mwm's.
  class StrictEqualF
  {
    PreResult2 const & m_r;
  public:
    StrictEqualF(PreResult2 const & r) : m_r(r) {}
    bool operator() (PreResult2 const & r) const;
  };

  /// To filter equal linear objects.
  //@{
  struct LessLinearTypesF
  {
    bool operator() (PreResult2 const & r1, PreResult2 const & r2) const;
  };
  class EqualLinearTypesF
  {
  public:
    bool operator() (PreResult2 const & r1, PreResult2 const & r2) const;
  };
  //@}

  string DebugPrint() const;

  bool IsStreet() const;

  inline FeatureID const & GetID() const { return m_id; }
  inline string const & GetName() const { return m_str; }
  inline feature::TypesHolder const & GetTypes() const { return m_types; }

private:
  template <class T> friend bool LessRankT(T const & r1, T const & r2);
  template <class T> friend bool LessViewportDistanceT(T const & r1, T const & r2);
  template <class T> friend bool LessDistanceT(T const & r1, T const & r2);

  bool IsEqualCommon(PreResult2 const & r) const;

  string GetFeatureType(CategoriesHolder const * pCat,
                        set<uint32_t> const * pTypes,
                        int8_t lang) const;

  FeatureID m_id;
  feature::TypesHolder m_types;

  uint32_t GetBestType(set<uint32_t> const * pPrefferedTypes = 0) const;

  string m_str;

  struct RegionInfo
  {
    string m_file;
    m2::PointD m_point;

    inline void SetParams(string const & file, m2::PointD const & pt)
    {
      m_file = file;
      m_point = pt;
    }

    void GetRegion(storage::CountryInfoGetter const * pInfo,
                   storage::CountryInfo & info) const;
  } m_region;

  m2::PointD GetCenter() const { return m_region.m_point; }

  double m_distance, m_distanceFromViewportCenter;
  ResultType m_resultType;
  uint8_t m_rank;
  uint8_t m_viewportDistance;
  feature::EGeomType m_geomType;
};

inline string DebugPrint(PreResult2 const & t)
{
  return t.DebugPrint();
}

}  // namespace search::impl
}  // namespace search
