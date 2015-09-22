#include "feature_generator.hpp"
#include "data_cache_file.hpp"
#include "osm_element.hpp"
#include "polygonizer.hpp"
#include "osm_decl.hpp"
#include "generate_info.hpp"
#include "coastlines_generator.hpp"
#include "world_map_generator.hpp"
#include "multiproducer_oneconsumer.hpp"

#include "../defines.hpp"

#include "../indexer/data_header.hpp"
#include "../indexer/mercator.hpp"
#include "../indexer/cell_id.hpp"
#include "../indexer/classificator.hpp"

#include "../coding/varint.hpp"
#include "../coding/mmap_reader.hpp"

#include "../base/assert.hpp"
#include "../base/logging.hpp"
#include "../base/stl_add.hpp"

#include "../std/bind.hpp"
#include "../std/unordered_map.hpp"
#include "../std/target_os.hpp"

namespace feature
{

template <class TNodesHolder>
class FileHolder : public cache::BaseFileHolder<TNodesHolder, cache::DataFileReader, FileReader>
{
  typedef cache::DataFileReader reader_t;
  typedef cache::BaseFileHolder<TNodesHolder, reader_t, FileReader> base_type;

  typedef typename base_type::offset_map_t offset_map_t;

  typedef typename base_type::user_id_t user_id_t;

  template <class TElement, class ToDo> struct process_base
  {
    reader_t & m_reader;
  protected:
    ToDo & m_toDo;
  public:
    process_base(reader_t & reader, ToDo & toDo) : m_reader(reader), m_toDo(toDo) {}

    bool operator() (uint64_t id)
    {
      TElement e;
      if (m_reader.Read(id, e))
        return m_toDo(id, e);
      return false;
    }
  };

  template <class ToDo> struct process_relation : public process_base<RelationElement, ToDo>
  {
    typedef process_base<RelationElement, ToDo> base_type;
  public:
    process_relation(reader_t & reader, ToDo & toDo) : base_type(reader, toDo) {}
  };

  template <class ToDo> struct process_relation_cached : public process_relation<ToDo>
  {
    typedef process_relation<ToDo> base_type;

  public:
    process_relation_cached(reader_t & rels, ToDo & toDo)
      : base_type(rels, toDo) {}

    bool operator() (uint64_t id)
    {
      switch (this->m_toDo(id))
      {
      case 1:   return true;
      case -1:  return false;
      default:  return base_type::operator()(id);
      }
    }
  };

public:
  FileHolder(TNodesHolder & holder, string const & dir) : base_type(holder, dir) {}

  bool GetNode(uint64_t id, double & lat, double & lng)
  {
    return this->m_nodes.GetPoint(id, lat, lng);
  }

  bool GetWay(user_id_t id, WayElement & e)
  {
    return this->m_ways.Read(id, e);
  }

  template <class ToDo> void ForEachRelationByWay(user_id_t id, ToDo & toDo)
  {
    process_relation<ToDo> processor(this->m_relations, toDo);
    this->m_ways2rel.for_each_ret(id, processor);
  }

  template <class ToDo> void ForEachRelationByNodeCached(user_id_t id, ToDo & toDo)
  {
    process_relation_cached<ToDo> processor(this->m_relations, toDo);
    this->m_nodes2rel.for_each_ret(id, processor);
  }

  template <class ToDo> void ForEachRelationByWayCached(user_id_t id, ToDo & toDo)
  {
    process_relation_cached<ToDo> processor(this->m_relations, toDo);
    this->m_ways2rel.for_each_ret(id, processor);
  }

  void LoadIndex()
  {
    this->m_ways.LoadOffsets();
    this->m_relations.LoadOffsets();

    this->m_nodes2rel.read_to_memory();
    this->m_ways2rel.read_to_memory();
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// FeaturesCollector implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

FeaturesCollector::FeaturesCollector(string const & fName)
: m_datFile(fName)
{
}

uint32_t FeaturesCollector::GetFileSize(FileWriter const & f)
{
  // .dat file should be less than 4Gb
  uint64_t const pos = f.Pos();
  uint32_t const ret = static_cast<uint32_t>(pos);

  CHECK_EQUAL(static_cast<uint64_t>(ret), pos, ("Feature offset is out of 32bit boundary!"));
  return ret;
}

void FeaturesCollector::WriteFeatureBase(vector<char> const & bytes, FeatureBuilder1 const & fb)
{
  size_t const sz = bytes.size();
  CHECK ( sz != 0, ("Empty feature not allowed here!") );

  if (sz > 0)
  {
    WriteVarUint(m_datFile, sz);
    m_datFile.Write(&bytes[0], sz);

    m_bounds.Add(fb.GetLimitRect());
  }
}

void FeaturesCollector::operator() (FeatureBuilder1 const & fb)
{
  // Just to ensure that file size is less than 4Gb.
  (void)GetFileSize(m_datFile);

  FeatureBuilder1::buffer_t bytes;
  fb.Serialize(bytes);
  WriteFeatureBase(bytes, fb);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Generate functions implementations.
///////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{

class points_in_file
{
#ifdef OMIM_OS_WINDOWS
  FileReader m_file;
#else
  MmapReader m_file;
#endif

public:
  points_in_file(string const & name) : m_file(name) {}

  bool GetPoint(uint64_t id, double & lat, double & lng) const
  {
    // I think, it's not good idea to write this ugly code.
    // memcpy isn't to slow for that.
//#ifdef OMIM_OS_WINDOWS
    LatLon ll;
    m_file.Read(id * sizeof(ll), &ll, sizeof(ll));
//#else
//    LatLon const & ll = *reinterpret_cast<LatLon const *>(m_file.Data() + id * sizeof(ll));
//#endif

    // assume that valid coordinate is not (0, 0)
    if (ll.lat != 0.0 || ll.lon != 0.0)
    {
      lat = ll.lat;
      lng = ll.lon;
      return true;
    }
    else
    {
      LOG(LERROR, ("Node with id = ", id, " not found!"));
      return false;
    }
  }
};

class points_in_map
{
  typedef unordered_map<uint64_t, pair<double, double> > cont_t;
  typedef cont_t::const_iterator iter_t;
  cont_t m_map;

  static bool equal_coord(double d1, double d2)
  {
    return ::fabs(d1 - d2) < 1.0E-8;
  }

public:
  points_in_map(string const & name)
  {
    LOG(LINFO, ("Nodes reading is started"));

    FileReader reader(name);
    uint64_t const count = reader.Size();

    uint64_t pos = 0;
    while (pos < count)
    {
      LatLonPos ll;
      reader.Read(pos, &ll, sizeof(ll));

      (void)m_map.insert(make_pair(ll.pos, make_pair(ll.lat, ll.lon)));

      pos += sizeof(ll);
    }

    LOG(LINFO, ("Nodes reading is finished"));
  }

  bool GetPoint(uint64_t id, double & lat, double & lng) const
  {
    iter_t i = m_map.find(id);
    if (i != m_map.end())
    {
      lat = i->second.first;
      lng = i->second.second;
      return true;
    }
    return false;
  }
};

class MainFeaturesEmitter
{
  scoped_ptr<Polygonizer<FeaturesCollector> > m_countries;
  scoped_ptr<WorldMapGenerator<FeaturesCollector> > m_world;
  scoped_ptr<CoastlineFeaturesGenerator> m_coasts;
  scoped_ptr<FeaturesCollector> m_coastsHolder;

  string m_srcCoastsFile;
  uint32_t m_coastType;

  template <class T1, class T2> class CombinedEmitter
  {
    T1 * m_p1;
    T2 * m_p2;
  public:
    CombinedEmitter(T1 * p1, T2 * p2) : m_p1(p1), m_p2(p2) {}
    void operator() (FeatureBuilder1 const & fb, uint64_t)
    {
      if (m_p1) (*m_p1)(fb);
      if (m_p2) (*m_p2)(fb);
    }
  };

public:
  MainFeaturesEmitter(GenerateInfo const & info)
  {
    m_coastType = classif().GetCoastType();

    m_srcCoastsFile = info.m_tmpDir + WORLD_COASTS_FILE_NAME + info.m_datFileSuffix;

    if (!info.m_makeCoasts)
    {
      m_countries.reset(new Polygonizer<FeaturesCollector>(info));

      if (info.m_emitCoasts)
      {
        m_coastsHolder.reset(new FeaturesCollector(
                info.m_datFilePrefix + WORLD_COASTS_FILE_NAME + info.m_datFileSuffix));
      }
    }
    else
    {
      // 4-10 - level range for cells
      // 20000 - max points count per feature
      m_coasts.reset(new CoastlineFeaturesGenerator(m_coastType, 4, 10, 20000));

      m_coastsHolder.reset(new FeaturesCollector(m_srcCoastsFile));
    }

    if (info.m_createWorld)
    {
      m_world.reset(new WorldMapGenerator<FeaturesCollector>(info));
    }
  }

  void operator() (FeatureBuilder1 fb)
  {
    if (m_coasts)
    {
      if (fb.HasType(m_coastType))
      {
        // leave only coastline type
        fb.SetType(m_coastType);
        (*m_coasts)(fb);
      }
    }

    // remove coastline type
    if (!fb.PopExactType(m_coastType) && fb.DoCorrect())
    {
      if (m_world)
        (*m_world)(fb);

      if (m_countries)
        (*m_countries)(fb);
    }
  }

private:
  class CoastFeatureTask : public MultiProducerOneConsumer::ITask
  {
    MainFeaturesEmitter & m_parent;
    size_t m_ind;

  public:
    CoastFeatureTask(MainFeaturesEmitter & parent, size_t ind)
      : m_parent(parent), m_ind(ind) {}

    virtual void RunBase()
    {
      vector<FeatureBuilder1> vecFb;
      m_parent.m_coasts->GetFeatures(m_ind, vecFb);

      for (size_t i = 0; i< vecFb.size(); ++i)
        Emit(&vecFb[i]);
    }

    virtual void EmitBase(void * p)
    {
      (*m_parent.m_coastsHolder)(*reinterpret_cast<FeatureBuilder1 *>(p));
    }
  };

public:
  void Finish()
  {
    if (m_world)
      m_world->DoMerge();

    if (m_coasts)
    {
      m_coasts->Finish();

      size_t const count = m_coasts->GetCellsCount();
      LOG(LINFO, ("Generating coastline features for ", count, " cells."));

      MultiProducerOneConsumer runner(8);
      for (size_t i = 0; i < count; ++i)
        runner.RunTask(new CoastFeatureTask(*this, i));
      runner.Finish();
    }
    else if (m_coastsHolder)
    {
      CombinedEmitter<
          FeaturesCollector,
          Polygonizer<FeaturesCollector> > emitter(m_coastsHolder.get(), m_countries.get());
      feature::ForEachFromDatRawFormat(m_srcCoastsFile, emitter);
    }
  }

  inline void GetNames(vector<string> & names) const
  {
    if (m_countries)
      names = m_countries->Names();
    else
      names.clear();
  }
};

}

template <class TNodesHolder>
bool GenerateImpl(GenerateInfo & info)
{
  try
  {
    TNodesHolder nodes(info.m_tmpDir + NODES_FILE);

    typedef FileHolder<TNodesHolder> holder_t;
    holder_t holder(nodes, info.m_tmpDir);
    holder.LoadIndex();

    MainFeaturesEmitter bucketer(info);
    SecondPassParserUsual<MainFeaturesEmitter, holder_t> parser(
          bucketer, holder, info.m_makeCoasts ? classif().GetCoastType() : 0);
    ParseXMLFromStdIn(parser);

    bucketer.Finish();
    bucketer.GetNames(info.m_bucketNames);
  }
  catch (Reader::Exception const & e)
  {
    LOG(LERROR, ("Error with file ", e.what()));
    return false;
  }

  return true;
}

bool GenerateFeatures(GenerateInfo & info, bool lightNodes)
{
  if (lightNodes)
    return GenerateImpl<points_in_map>(info);
  else
    return GenerateImpl<points_in_file>(info);
}

}
