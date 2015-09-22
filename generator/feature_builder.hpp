#pragma once

#include "osm_id.hpp"

#include "../indexer/feature.hpp"

#include "../coding/file_reader.hpp"

#include "../std/bind.hpp"


namespace serial { class CodingParams; }

/// Used for serialization\deserialization of features during --generate_features.
class FeatureBuilder1
{
  /// For debugging
  friend string DebugPrint(FeatureBuilder1 const & f);

public:
  FeatureBuilder1();

  /// @name Geometry manipulating functions.
  //@{
  /// Set center (origin) point of feature and set that feature is point.
  void SetCenter(m2::PointD const & p);

  /// Add point to geometry.
  void AddPoint(m2::PointD const & p);

  /// Set that feature is linear type.
  void SetLinear() { m_Params.SetGeomType(feature::GEOM_LINE); }
  void SetArea() { m_Params.SetGeomType(feature::GEOM_AREA); }

  /// Set that feature is area and get ownership of holes.
  void SetAreaAddHoles(list<vector<m2::PointD> > const & holes);

  void AddPolygon(vector<m2::PointD> & poly);
  //@}

  inline feature::EGeomType GetGeomType() const { return m_Params.GetGeomType(); }

  inline void AddType(uint32_t type) { m_Params.AddType(type); }
  inline bool HasType(uint32_t t) const { return m_Params.IsTypeExist(t); }
  inline bool PopExactType(uint32_t type) { return m_Params.PopExactType(type); }
  inline void SetType(uint32_t type) { m_Params.SetType(type); }

  /// Check for feature visibility according to it's types.
  /// If feature is invisible, it's not correct.
  /// This fuction is called after it's classificator types manipulating.
  void DoCorrectForType(feature::EGeomType type);
  bool DoCorrect();

  typedef vector<char> buffer_t;

  /// @name Serialization.
  //@{
  void Serialize(buffer_t & data) const;
  void SerializeBase(buffer_t & data, serial::CodingParams const & params) const;

  void Deserialize(buffer_t & data);
  //@}

  /// @name Selectors.
  //@{
  inline m2::RectD GetLimitRect() const { return m_LimitRect; }

  /// Get common parameters of feature.
  FeatureBase GetFeatureBase() const;

  bool IsGeometryClosed() const;

  inline size_t GetPointsCount() const { return GetGeometry().size(); }
  inline size_t GetPolygonsCount() const { return m_Polygons.size(); }
  //@}

  /// @name Iterate through polygons points.
  /// Stops processing when functor returns false.
  //@{
private:
  template <class ToDo> class ToDoWrapper
  {
    ToDo & m_toDo;
  public:
    ToDoWrapper(ToDo & toDo) : m_toDo(toDo) {}
    bool operator() (m2::PointD const & p) { return m_toDo(p); }
    void EndRegion() {}
  };

public:
  template <class ToDo>
  void ForEachGeometryPointEx(ToDo & toDo) const
  {
    if (m_Params.GetGeomType() == feature::GEOM_POINT)
      toDo(m_Center);
    else
    {
      for (list<points_t>::const_iterator i = m_Polygons.begin(); i != m_Polygons.end(); ++i)
      {
        for (points_t::const_iterator j = i->begin(); j != i->end(); ++j)
          if (!toDo(*j))
            return;
        toDo.EndRegion();
      }
    }
  }

  template <class ToDo>
  void ForEachGeometryPoint(ToDo & toDo) const
  {
    ToDoWrapper<ToDo> wrapper(toDo);
    ForEachGeometryPointEx(wrapper);
  }
  //@}

  bool PreSerialize();

  /// @note This function overrides all previous assigned types.
  /// Set all the parameters, except geometry type (it's set by other functions).
  inline void SetParams(FeatureParams const & params) { m_Params.SetParams(params); }

  /// For OSM debugging, store original OSM id
  void AddOsmId(string const & type, uint64_t osmId);
  string GetOsmIdsString() const;

  int GetMinFeatureDrawScale() const;

  void SetCoastCell(int64_t iCell, string const & strCell);
  inline bool GetCoastCell(int64_t & cell) const
  {
    if (m_coastCell != -1)
    {
      cell = m_coastCell;
      return true;
    }
    else return false;
  }

  inline string GetName() const
  {
    string s;
    m_Params.name.GetString(0, s);
    return s;
  }

  /// @name For diagnostic use only.
  //@{
  bool operator== (FeatureBuilder1 const &) const;

  bool CheckValid() const;
  //@}

protected:
  /// Used for feature debugging
  vector<osm::OsmId> m_osmIds;

  FeatureParams m_Params;

  m2::RectD m_LimitRect;

  /// Can be one of the following:
  /// - point in point-feature
  /// - origin point of text [future] in line-feature
  /// - origin point of text or symbol in area-feature
  m2::PointD m_Center;    // Check  HEADER_HAS_POINT

  typedef vector<m2::PointD> points_t;

  inline points_t const & GetGeometry() const { return m_Polygons.front(); }

  /// List of geometry polygons.
  list<points_t> m_Polygons; // Check HEADER_IS_AREA

  int64_t m_coastCell;
};

/// Used for serialization of features during final pass.
class FeatureBuilder2 : public FeatureBuilder1
{
  typedef FeatureBuilder1 base_type;

  typedef vector<uint32_t> offsets_t;

  static void SerializeOffsets(uint32_t mask, offsets_t const & offsets, buffer_t & buffer);

public:

  struct buffers_holder_t
  {
    /// @name input
    //@{
    offsets_t m_ptsOffset, m_trgOffset;
    uint8_t m_ptsMask, m_trgMask;

    uint32_t m_ptsSimpMask;

    points_t m_innerPts, m_innerTrg;
    //@}

    /// @name output
    base_type::buffer_t m_buffer;

    buffers_holder_t() : m_ptsMask(0), m_trgMask(0), m_ptsSimpMask(0) {}
  };

  bool IsLine() const { return (m_Params.GetTypeMask() & feature::HEADER_GEOM_LINE) != 0; }
  bool IsArea() const { return (m_Params.GetTypeMask() & feature::HEADER_GEOM_AREA) != 0; }
  bool IsDrawableInRange(int lowS, int highS) const;

  points_t const & GetOuterPoly() const { return GetGeometry(); }
  list<points_t> const & GetPolygons() const { return m_Polygons; }

  /// @name Overwrite from base_type.
  //@{
  bool PreSerialize(buffers_holder_t const & data);
  void Serialize(buffers_holder_t & data, serial::CodingParams const & params);
  //@}
};

namespace feature
{
  /// Read feature from feature source.
  template <class TSource>
  void ReadFromSourceRowFormat(TSource & src, FeatureBuilder1 & fb)
  {
   uint32_t const sz = ReadVarUint<uint32_t>(src);
   typename FeatureBuilder1::buffer_t buffer(sz);
   src.Read(&buffer[0], sz);
   fb.Deserialize(buffer);
  }

  /// Process features in .dat file.
  template <class ToDo>
  void ForEachFromDatRawFormat(string const & fName, ToDo & toDo)
  {
   FileReader reader(fName);
   ReaderSource<FileReader> src(reader);

   uint64_t currPos = 0;
   uint64_t const fSize = reader.Size();

   // read features one by one
   while (currPos < fSize)
   {
     FeatureBuilder1 fb;
     ReadFromSourceRowFormat(src, fb);
     toDo(fb, currPos);
     currPos = src.Pos();
   }
  }
}
