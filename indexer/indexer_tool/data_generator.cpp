#include "data_generator.hpp"
#include "data_cache_file.hpp"
#include "first_pass_parser.hpp"

#include "../../indexer/osm_decl.hpp"

#include "../../base/std_serialization.hpp"
#include "../../base/logging.hpp"

#include "../../std/bind.hpp"

#include "../../base/start_mem_debug.hpp"


namespace data
{

template <class TNodesHolder>
class FileHolder : public cache::BaseFileHolder<TNodesHolder, cache::DataFileWriter, FileWriter>
{
  typedef cache::BaseFileHolder<TNodesHolder, cache::DataFileWriter, FileWriter> base_type;

  typedef typename base_type::user_id_t user_id_t;

  template <class TMap, class TVec>
  void add_id2rel_vector(TMap & rMap, user_id_t relid, TVec const & v)
  {
    for (size_t i = 0; i < v.size(); ++i)
      rMap.write(v[i].first, relid);
  }

public:
  FileHolder(TNodesHolder & nodes, string const & dir) : base_type(nodes, dir) {}

  void AddNode(uint64_t id, double lat, double lng)
  {
    this->m_nodes.AddPoint(id, lat, lng);
  }

  void AddWay(user_id_t id, WayElement const & e)
  {
    this->m_ways.Write(id, e);
  }

  void AddRelation(user_id_t id, RelationElement const & e)
  {
    this->m_relations.Write(id, e);

    add_id2rel_vector(this->m_nodes2rel, id, e.nodes);
    add_id2rel_vector(this->m_ways2rel, id, e.ways);
  }

  void AddMappedWay(user_id_t id, WayElement const & e, bool emptyTags)
  {
    typedef cache::MappedWay way_t;

    int const type = (emptyTags ? way_t::empty_direct : way_t::coast_direct);

    this->m_mappedWays.write(e.nodes.front(),  way_t(id, type));     // direct
    this->m_mappedWays.write(e.nodes.back(), way_t(id, type + 2));   // opposite
  }

  void SaveIndex()
  {
    this->m_ways.SaveOffsets();
    this->m_relations.SaveOffsets();

    this->m_nodes2rel.flush_to_file();
    this->m_ways2rel.flush_to_file();
    this->m_mappedWays.flush_to_file();
  }
};


class points_in_file_base
{
protected:
  FileWriter m_file;
  progress_policy m_progress;

public:
  points_in_file_base(string const & name, size_t factor) : m_file(name.c_str())
  {
    m_progress.Begin(name, factor);
  }

  uint64_t GetCount() const { return m_progress.GetCount(); }
};

class points_in_file : public points_in_file_base
{
public:
  points_in_file(string const & name) : points_in_file_base(name, 1000) {}

  void AddPoint(uint64_t id, double lat, double lng)
  {
    LatLon ll;
    ll.lat = lat;
    ll.lon = lng;
    m_file.Seek(id * sizeof(ll));
    m_file.Write(&ll, sizeof(ll));

    m_progress.Inc();
  }
};

class points_in_file_light : public points_in_file_base
{
public:
  points_in_file_light(string const & name) : points_in_file_base(name, 10000) {}

  void AddPoint(uint64_t id, double lat, double lng)
  {
    LatLonPos ll;
    ll.pos = id;
    ll.lat = lat;
    ll.lon = lng;
    m_file.Write(&ll, sizeof(ll));

    m_progress.Inc();
  }
};

template <class TNodesHolder>
bool GenerateImpl(string const & dir)
{
  try
  {
    TNodesHolder nodes(dir + NODES_FILE);
    typedef FileHolder<TNodesHolder> holder_t;
    holder_t holder(nodes, dir);

    FirstPassParser<holder_t> parser(holder);
    ParseXMLFromStdIn(parser);

    LOG(LINFO, ("Added points count = ", nodes.GetCount()));

    holder.SaveIndex();
  }
  catch (Writer::Exception const & e)
  {
    LOG(LERROR, ("Error with file ", e.what()));
    return false;
  }

  return true;
}

bool GenerateToFile(string const & dir, bool lightNodes)
{
  if (lightNodes)
    return GenerateImpl<points_in_file_light>(dir);
  else
    return GenerateImpl<points_in_file>(dir);
}

}
