#pragma once
#include "indexer/feature_decl.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include "base/buffer_vector.hpp"

#include "std/string.hpp"


namespace search
{

// Search result. Search returns a list of them, ordered by score.
class Result
{
public:
  enum ResultType
  {
    RESULT_FEATURE,
    RESULT_LATLON,
    RESULT_SUGGEST_PURE,
    RESULT_SUGGEST_FROM_FEATURE
  };

  /// For RESULT_FEATURE.
  Result(FeatureID const & id, m2::PointD const & pt,
         string const & str, string const & region,
         string const & type, uint32_t featureType);

  /// Used for point-like results on the map.
  Result(m2::PointD const & pt, string const & str, string const & type);

  /// @param[in] type Pass 0 - for RESULT_LATLON or building type for building address.
  Result(m2::PointD const & pt, string const & str,
         string const & region, string const & type);

  /// For RESULT_SUGGESTION_PURE.
  Result(string const & str, string const & suggest);

  /// For RESULT_SUGGESTION_FROM_FEATURE.
  Result(Result const & res, string const & suggest);

  /// Strings that is displayed in the GUI.
  //@{
  char const * GetString() const { return m_str.c_str(); }
  char const * GetRegionString() const { return m_region.c_str(); }
  char const * GetFeatureType() const { return m_type.c_str(); }
  //@}

  bool IsSuggest() const;
  bool HasPoint() const;

  /// Type of the result.
  ResultType GetResultType() const;

  /// Feature id in mwm.
  /// @precondition GetResultType() == RESULT_FEATURE
  FeatureID GetFeatureID() const;

  /// Center point of a feature.
  /// @precondition HasPoint() == true
  m2::PointD GetFeatureCenter() const;

  /// String to write in the search box.
  /// @precondition IsSuggest() == true
  char const * GetSuggestionString() const;

  bool operator== (Result const & r) const;

  void AddHighlightRange(pair<uint16_t, uint16_t> const & range);
  pair<uint16_t, uint16_t> const & GetHighlightRange(size_t idx) const;
  inline size_t GetHighlightRangesCount() const { return m_hightlightRanges.size(); }

  void AppendCity(string const & name);

private:
  FeatureID m_id;
  m2::PointD m_center;
  string m_str, m_region, m_type;
  uint32_t m_featureType;
  string m_suggestionStr;
  buffer_vector<pair<uint16_t, uint16_t>, 4> m_hightlightRanges;
};

class Results
{
  vector<Result> m_vec;

  enum StatusT {
    NONE,             // default status
    ENDED_CANCELLED,  // search ended with canceling
    ENDED             // search ended itself
  };
  StatusT m_status;

  explicit Results(bool isCancelled)
  {
    m_status = (isCancelled ? ENDED_CANCELLED : ENDED);
  }

public:
  Results() : m_status(NONE) {}

  /// @name To implement end of search notification.
  //@{
  static Results GetEndMarker(bool isCancelled) { return Results(isCancelled); }
  bool IsEndMarker() const { return (m_status != NONE); }
  bool IsEndedNormal() const { return (m_status == ENDED); }
  //@}

  inline bool AddResult(Result const & r)
  {
    m_vec.push_back(r);
    return true;
  }
  bool AddResultCheckExisting(Result const & r);

  inline void Clear() { m_vec.clear(); }

  typedef vector<Result>::const_iterator IterT;
  inline IterT Begin() const { return m_vec.begin(); }
  inline IterT End() const { return m_vec.end(); }

  inline size_t GetCount() const { return m_vec.size(); }
  size_t GetSuggestsCount() const;

  inline Result const & GetResult(size_t i) const
  {
    ASSERT_LESS(i, m_vec.size(), ());
    return m_vec[i];
  }

  inline void Swap(Results & rhs)
  {
    m_vec.swap(rhs.m_vec);
  }

  template <class LessT> void Sort(LessT lessFn)
  {
    sort(m_vec.begin(), m_vec.end(), lessFn);
  }
};

struct AddressInfo
{
  string m_country, m_city, m_street, m_house, m_name;
  vector<string> m_types;
  // TODO(AlexZ): It is not initialized in MakeFrom() below because we don't need it in search at the moment.
  bool m_isBuilding = false;

  void MakeFrom(search::Result const & res);

  string GetPinName() const;    // Caroline
  string GetPinType() const;    // shop

  string FormatPinText() const; // Caroline (clothes shop)
  string FormatAddress() const; // 7 vulica Frunze, Belarus
  string FormatTypes() const;   // clothes shop
  string FormatNameAndAddress() const;  // Caroline, 7 vulica Frunze, Belarus
  char const * GetBestType() const;
  bool IsEmptyName() const;
  bool IsBuilding() const { return m_isBuilding; }

  void Clear();
};

}
