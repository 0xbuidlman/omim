#pragma once

#include "geometry_coding.hpp"
#include "tesselator_decl.hpp"

#include "../geometry/point2d.hpp"

#include "../coding/reader.hpp"
#include "../coding/writer.hpp"
#include "../coding/varint.hpp"

#include "../std/algorithm.hpp"
#include "../std/bind.hpp"

#include "../base/buffer_vector.hpp"
#include "../base/stl_add.hpp"

//class FileReader;
//class FileWriter;

namespace serial
{
  class CodingParams
  {
  public:
    // TODO: Factor out?
    CodingParams();
    CodingParams(uint8_t coordBits, m2::PointD const & pt);
    CodingParams(uint8_t coordBits, uint64_t basePointUint64);

    m2::PointU GetBasePointPrediction(uint64_t offset) const;

    // TODO: Factor out.
    m2::PointU GetBasePoint() const { return m_BasePoint; }
    // TODO: Factor out.
    int64_t GetBasePointInt64() const { return static_cast<int64_t>(m_BasePointUint64); }

    uint32_t GetCoordBits() const { return m_CoordBits; }

    template <typename WriterT> void Save(WriterT & writer) const
    {
      WriteVarUint(writer, GetCoordBits());
      WriteVarUint(writer, static_cast<uint64_t>(GetBasePointInt64()));
    }

    template <typename SourceT> void Load(SourceT & src)
    {
      uint32_t const coordBits = ReadVarUint<uint32_t>(src);
      ASSERT_LESS(coordBits, 32, ());
      uint64_t const basePointUint64 = ReadVarUint<uint64_t>(src);
      *this = CodingParams(coordBits, basePointUint64);
    }

  private:
    uint64_t m_BasePointUint64;
    // TODO: Factor out.
    m2::PointU m_BasePoint;
    uint8_t m_CoordBits;
  };

  template <class TCont, class TSink>
  inline void WriteVarUintArray(TCont const & v, TSink & sink)
  {
    for (size_t i = 0; i != v.size(); ++i)
      WriteVarUint(sink, v[i]);
  }

  /// @name Encode and Decode function types.
  //@{
  typedef void (*EncodeFunT)( geo_coding::InPointsT const &,
                              m2::PointU const &, m2::PointU const &,
                              geo_coding::OutDeltasT &);
  typedef void (*DecodeFunT)( geo_coding::InDeltasT const &,
                              m2::PointU const &, m2::PointU const &,
                              geo_coding::OutPointsT &);
  //@}

  typedef buffer_vector<uint64_t, 32> DeltasT;
  typedef buffer_vector<m2::PointD, 32> OutPointsT;

  void Encode(EncodeFunT fn, vector<m2::PointD> const & points, CodingParams const & params,
              DeltasT & deltas);

  /// @name Overloads for different out container types.
  //@{
  void Decode(DecodeFunT fn, DeltasT const & deltas, CodingParams const & params,
              OutPointsT & points, size_t reserveF = 1);
  void Decode(DecodeFunT fn, DeltasT const & deltas, CodingParams const & params,
              vector<m2::PointD> & points, size_t reserveF = 1);
  //@}

  template <class TSink>
  void SaveInner(EncodeFunT fn, vector<m2::PointD> const & points,
                 CodingParams const & params, TSink & sink)
  {
    DeltasT deltas;
    Encode(fn, points, params, deltas);
    WriteVarUintArray(deltas, sink);
  }

  template <class TSink>
  void WriteBufferToSink(vector<char> const & buffer, TSink & sink)
  {
    uint32_t const count = buffer.size();
    WriteVarUint(sink, count);
    sink.Write(&buffer[0], count);
  }

  template <class TSink>
  void SaveOuter(EncodeFunT fn, vector<m2::PointD> const & points,
                 CodingParams const & params, TSink & sink)
  {
    DeltasT deltas;
    Encode(fn, points, params, deltas);

    vector<char> buffer;
    MemWriter<vector<char> > writer(buffer);
    WriteVarUintArray(deltas, writer);

    WriteBufferToSink(buffer, sink);
  }

  void const * LoadInner(DecodeFunT fn, void const * pBeg, size_t count,
                         CodingParams const & params, OutPointsT & points);

  template <class TSource, class TPoints>
  void LoadOuter(DecodeFunT fn, TSource & src, CodingParams const & params,
                 TPoints & points, size_t reserveF = 1)
  {
    uint32_t const count = ReadVarUint<uint32_t>(src);
    vector<char> buffer(count);
    char * p = &buffer[0];
    src.Read(p, count);

    DeltasT deltas;
    deltas.reserve(count / 2);
    ReadVarUint64Array(p, p + count, MakeBackInsertFunctor(deltas));

    Decode(fn, deltas, params, points, reserveF);
  }


  /// @name Paths.
  //@{
  template <class TSink>
  void SaveInnerPath(vector<m2::PointD> const & points, CodingParams const & params, TSink & sink)
  {
    SaveInner(&geo_coding::EncodePolyline, points, params, sink);
  }
  template <class TSink>
  void SaveOuterPath(vector<m2::PointD> const & points, CodingParams const & params, TSink & sink)
  {
    SaveOuter(&geo_coding::EncodePolyline, points, params, sink);
  }

  inline void const * LoadInnerPath(void const * pBeg, size_t count, CodingParams const & params,
                                    OutPointsT & points)
  {
    return LoadInner(&geo_coding::DecodePolyline, pBeg, count, params, points);
  }

  template <class TSource, class TPoints>
  void LoadOuterPath(TSource & src, CodingParams const & params, TPoints & points)
  {
    LoadOuter(&geo_coding::DecodePolyline, src, params, points);
  }
  //@}

  /// @name Triangles.
  //@{
  template <class TSink>
  void SaveInnerTriangles(vector<m2::PointD> const & points,
                          CodingParams const & params, TSink & sink)
  {
    SaveInner(&geo_coding::EncodeTriangleStrip, points, params, sink);
  }

  inline void const * LoadInnerTriangles(void const * pBeg, size_t count,
                                         CodingParams const & params, OutPointsT & points)
  {
    return LoadInner(&geo_coding::DecodeTriangleStrip, pBeg, count, params, points);
  }

  class TrianglesChainSaver
  {
    typedef m2::PointU PointT;
    typedef tesselator::Edge EdgeT;
    typedef vector<char> BufferT;

    PointT m_base, m_max;

    list<BufferT> m_buffers;

  public:
    explicit TrianglesChainSaver(CodingParams const & params);

    PointT GetBasePoint() const { return m_base; }
    PointT GetMaxPoint() const { return m_max; }

    void operator() (PointT arr[3], vector<EdgeT> edges);

    size_t GetBufferSize() const
    {
      size_t sz = 0;
      for (list<BufferT>::const_iterator i = m_buffers.begin(); i != m_buffers.end(); ++i)
        sz += i->size();
      return sz;
    }

    template <class TSink> void Save(TSink & sink)
    {
      // assume that 2 byte is enough for triangles count
      size_t const count = m_buffers.size();
      CHECK_LESS_OR_EQUAL(count, 0x3FFF, ());

      WriteVarUint(sink, static_cast<uint32_t>(count));

      for_each(m_buffers.begin(), m_buffers.end(), bind(&WriteBufferToSink<TSink>, _1, ref(sink)));
    }
  };

  void DecodeTriangles(geo_coding::InDeltasT const & deltas,
                      m2::PointU const & basePoint,
                      m2::PointU const & maxPoint,
                      geo_coding::OutPointsT & triangles);

  template <class TSource>
  void LoadOuterTriangles(TSource & src, CodingParams const & params, OutPointsT & triangles)
  {
    int const count = ReadVarUint<uint32_t>(src);

    for (int i = 0; i < count; ++i)
      LoadOuter(&DecodeTriangles, src, params, triangles, 3);
  }
  //@}
}
