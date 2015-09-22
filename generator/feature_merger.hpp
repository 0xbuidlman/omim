#pragma once
#include "feature_emitter_iface.hpp"

#include "../indexer/feature.hpp"

#include "../std/map.hpp"
#include "../std/vector.hpp"


/// Feature builder class that used while feature type processing and merging.
class MergedFeatureBuilder1 : public FeatureBuilder1
{
  bool m_isRound;

  points_t m_roundBounds[2];

public:
  MergedFeatureBuilder1() : m_isRound(false) {}
  MergedFeatureBuilder1(FeatureBuilder1 const & fb);

  void SetRound();
  bool IsRound() const { return m_isRound; }

  void AppendFeature(MergedFeatureBuilder1 const & fb, bool fromBegin, bool toBack);

  bool EqualGeometry(MergedFeatureBuilder1 const & fb) const;

  inline bool NotEmpty() const { return !m_Geometry.empty(); }

  inline m2::PointD FirstPoint() const { return m_Geometry.front(); }
  inline m2::PointD LastPoint() const { return m_Geometry.back(); }

  inline void SetType(uint32_t type) { m_Params.SetType(type); }
  inline bool PopAnyType(uint32_t & type) { return m_Params.PopAnyType(type); }
  inline bool PopExactType(uint32_t type) { return m_Params.PopExactType(type); }

  template <class ToDo> void ForEachChangeTypes(ToDo toDo)
  {
    for_each(m_Params.m_Types.begin(), m_Params.m_Types.end(), toDo);
    m_Params.FinishAddingTypes();
  }

  template <class ToDo> void ForEachMiddlePoints(ToDo toDo) const
  {
    for (size_t i = 1; i < m_Geometry.size()-1; ++i)
      toDo(m_Geometry[i]);
  }

  pair<m2::PointD, bool> GetKeyPoint(size_t i) const;
  size_t GetKeyPointsCount() const;

  double GetPriority() const;
};

/// Feature merger.
class FeatureMergeProcessor
{
  typedef int64_t key_t;
  key_t get_key(m2::PointD const & p);

  MergedFeatureBuilder1 m_last;

  typedef vector<MergedFeatureBuilder1 *> vector_t;
  typedef map<key_t, vector_t> map_t;
  map_t m_map;

  void Insert(m2::PointD const & pt, MergedFeatureBuilder1 * p);

  void Remove(key_t key, MergedFeatureBuilder1 const * p);
  inline void Remove1(m2::PointD const & pt, MergedFeatureBuilder1 const * p)
  {
    Remove(get_key(pt), p);
  }
  void Remove(MergedFeatureBuilder1 const * p);

  uint32_t m_coordBits;

public:
  FeatureMergeProcessor(uint32_t coordBits);

  void operator() (FeatureBuilder1 const & fb);
  void operator() (MergedFeatureBuilder1 * p);

  void DoMerge(FeatureEmitterIFace & emitter);
};


/// Feature types corrector.
class FeatureTypesProcessor
{
  map<uint32_t, uint32_t> m_mapping;

  static uint32_t GetType(char const * arr[2]);

  void CorrectType(uint32_t & t) const;

  class do_change_types
  {
    FeatureTypesProcessor const & m_pr;
  public:
    do_change_types(FeatureTypesProcessor const & pr) : m_pr(pr) {}
    void operator() (uint32_t & t) { m_pr.CorrectType(t); }
  };

public:
  /// For example: highway-motorway_link => highway-motorway
  void SetMappingTypes(char const * arr1[2], char const * arr2[2]);

  MergedFeatureBuilder1 * operator() (FeatureBuilder1 const & fb);
};
