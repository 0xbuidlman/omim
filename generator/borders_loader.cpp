#include "borders_loader.hpp"
#include "borders_generator.hpp"

#include "../defines.hpp"

#include "../storage/country_polygon.hpp"

#include "../indexer/geometry_serialization.hpp"
#include "../indexer/scales.hpp"

#include "../geometry/simplification.hpp"
#include "../geometry/distance.hpp"

#include "../coding/file_container.hpp"
#include "../coding/read_write_utils.hpp"

#include "../base/logging.hpp"
#include "../base/string_utils.hpp"

#include "../std/fstream.hpp"
#include "../std/vector.hpp"
#include "../std/bind.hpp"


#define BORDERS_DIR "borders/"
#define BORDERS_EXTENSION ".borders"
#define POLYGONS_FILE "polygons.lst"


namespace borders
{
  class PolygonLoader
  {
    CountryPolygons m_polygons;
    m2::RectD m_rect;

    string const & m_baseDir;
    CountriesContainerT & m_countries;

  public:
    PolygonLoader(string const & baseDir, CountriesContainerT & countries)
      : m_baseDir(baseDir), m_countries(countries) {}

    void operator() (string const & name)
    {
      if (m_polygons.m_name.empty())
        m_polygons.m_name = name;

      vector<m2::RegionD> borders;
      if (osm::LoadBorders(m_baseDir + BORDERS_DIR + name + BORDERS_EXTENSION, borders))
      {
        for (size_t i = 0; i < borders.size(); ++i)
        {
          m2::RectD const rect(borders[i].GetRect());
          m_rect.Add(rect);
          m_polygons.m_regions.Add(borders[i], rect);
        }
      }
    }

    void Finish()
    {
      if (!m_polygons.IsEmpty())
      {
        ASSERT_NOT_EQUAL ( m_rect, m2::RectD::GetEmptyRect(), () );
        m_countries.Add(m_polygons, m_rect);
      }

      m_polygons.Clear();
      m_rect.MakeEmpty();
    }
  };

  template <class ToDo>
  void ForEachCountry(string const & baseDir, ToDo & toDo)
  {
    ifstream stream((baseDir + POLYGONS_FILE).c_str());
    string line;

    while (stream.good())
    {
      std::getline(stream, line);
      if (line.empty())
        continue;

      /// @todo no need to tokenize by '|'
      strings::Tokenize(line, "|", bind<void>(ref(toDo), _1));
      toDo.Finish();
    }
  }

  bool LoadCountriesList(string const & baseDir, CountriesContainerT & countries)
  {
    countries.Clear();

    LOG(LINFO, ("Loading countries."));

    PolygonLoader loader(baseDir, countries);
    ForEachCountry(baseDir, loader);

    LOG(LINFO, ("Countries loaded:", countries.GetSize()));

    return !countries.IsEmpty();
  }

  class PackedBordersGenerator
  {
    FilesContainerW m_writer;
    string const & m_baseDir;

    vector<storage::CountryDef> m_polys;

  public:
    PackedBordersGenerator(string const & baseDir)
      : m_writer(baseDir + PACKED_POLYGONS_FILE), m_baseDir(baseDir)
    {
    }

    void operator() (string const & name)
    {
      vector<m2::RegionD> borders;
      if (osm::LoadBorders(m_baseDir + BORDERS_DIR + name + BORDERS_EXTENSION, borders))
      {
        // use index in vector as tag
        FileWriter w = m_writer.GetWriter(strings::to_string(m_polys.size()));
        serial::CodingParams cp;

        uint32_t const count = static_cast<uint32_t>(borders.size());

        // calc rect
        m2::RectD rect;
        for (uint32_t i = 0; i < count; ++i)
          rect.Add(borders[i].GetRect());

        // store polygon info
        m_polys.push_back(storage::CountryDef(name, rect));

        // write polygons as paths
        WriteVarUint(w, count);
        for (uint32_t i = 0; i < count; ++i)
        {
          typedef vector<m2::PointD> VectorT;
          typedef mn::DistanceToLineSquare<m2::PointD> DistanceT;

          VectorT const & in = borders[i].Data();
          VectorT out;

          /// @todo Choose scale level for simplification.
          double const eps = my::sq(scales::GetEpsilonForSimplify(10));
          DistanceT dist;
          SimplifyNearOptimal(20, in.begin(), in.end(), eps, dist,
                              AccumulateSkipSmallTrg<DistanceT, m2::PointD>(dist, out, eps));

          serial::SaveOuterPath(out, cp, w);
        }
      }
    }

    void Finish() {}

    void WritePolygonsInfo()
    {
      FileWriter w = m_writer.GetWriter(PACKED_POLYGONS_INFO_TAG);
      rw::Write(w, m_polys);
    }
  };

  void GeneratePackedBorders(string const & baseDir)
  {
    PackedBordersGenerator generator(baseDir);
    ForEachCountry(baseDir, generator);
    generator.WritePolygonsInfo();
  }
}
