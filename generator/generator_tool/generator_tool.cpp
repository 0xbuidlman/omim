#include "../data_generator.hpp"
#include "../feature_generator.hpp"
#include "../feature_sorter.hpp"
#include "../update_generator.hpp"
#include "../borders_generator.hpp"
#include "../borders_loader.hpp"
#include "../dumper.hpp"
#include "../statistics.hpp"
#include "../unpack_mwm.hpp"
#include "../generate_info.hpp"

#include "../../indexer/drawing_rules.hpp"
#include "../../indexer/classificator_loader.hpp"
#include "../../indexer/classificator.hpp"
#include "../../indexer/data_header.hpp"
#include "../../indexer/features_vector.hpp"
#include "../../indexer/index_builder.hpp"
#include "../../indexer/search_index_builder.hpp"

#include "../../defines.hpp"

#include "../../platform/platform.hpp"

#include "../../3party/gflags/src/gflags/gflags.h"

#include "../../std/iostream.hpp"
#include "../../std/iomanip.hpp"
#include "../../std/numeric.hpp"

#include "../../version/version.hpp"


DEFINE_bool(version, false, "Display version");
DEFINE_bool(generate_update, false,
              "If specified, update.maps file will be generated from cells in the data path");

#ifndef OMIM_PRODUCTION
DEFINE_bool(generate_classif, false, "Generate classificator.");
#endif

DEFINE_bool(preprocess_xml, false, "1st pass - create nodes/ways/relations data");
DEFINE_bool(make_coasts, false, "create intermediate file with coasts data");
DEFINE_bool(emit_coasts, false, "push coasts features from intermediate file to out files/countries");

DEFINE_bool(generate_features, false, "2nd pass - generate intermediate features");
DEFINE_bool(generate_geometry, false, "3rd pass - split and simplify geometry and triangles for features");
DEFINE_bool(generate_index, false, "4rd pass - generate index");
DEFINE_bool(generate_search_index, false, "5th pass - generate search index");
DEFINE_bool(calc_statistics, false, "Calculate feature statistics for specified mwm bucket files");
DEFINE_bool(use_light_nodes, false,
            "If true, use temporary vector of nodes, instead of huge temp file");
DEFINE_string(data_path, "", "Working directory, 'path_to_exe/../../data' if empty.");
DEFINE_string(output, "", "Prefix of filenames of outputted .dat and .idx files.");
DEFINE_string(intermediate_data_path, "", "Path to store nodes, ways, relations.");
DEFINE_bool(generate_world, false, "Generate separate world file");
DEFINE_bool(split_by_polygons, false, "Use countries borders to split planet by regions and countries");
DEFINE_string(generate_borders, "",
            "Create binary country .borders file for osm xml file given in 'output' parameter,"
            "specify tag name and optional value: ISO3166-1 or admin_level=4");
DEFINE_bool(dump_types, false, "If defined, prints all types combinations and their total count");
DEFINE_bool(dump_prefixes, false, "If defined, prints statistics on feature name prefixes");
DEFINE_bool(dump_search_tokens, false, "Print statistics on search tokens.");
DEFINE_bool(unpack_mwm, false, "Unpack each section of mwm into a separate file with name filePath.sectionName.");
DEFINE_bool(generate_packed_borders, false, "Generate packed file with country polygons.");
DEFINE_string(delete_section, "", "Delete specified section (defines.hpp) from container.");

string AddSlashIfNeeded(string const & str)
{
  string result(str);
  size_t const size = result.size();
  if (size)
  {
    if (result.find_last_of('\\') == size - 1)
      result[size - 1] = '/';
    else
      if (result.find_last_of('/') != size - 1)
        result.push_back('/');
  }
  return result;
}

int main(int argc, char ** argv)
{
  google::SetUsageMessage(
      "Takes OSM XML data from stdin and creates data and index files in several passes.");

  google::ParseCommandLineFlags(&argc, &argv, true);

  Platform & pl = GetPlatform();

  string const path =
      FLAGS_data_path.empty() ? pl.WritableDir() : AddSlashIfNeeded(FLAGS_data_path);

  if (FLAGS_version)
  {
    cout << "Tool version: " << VERSION_STRING << endl;
    cout << "Built on: " << VERSION_DATE_STRING << endl;
  }

#ifndef OMIM_PRODUCTION
  // Make a classificator
  if (FLAGS_generate_classif)
  {
    drule::RulesHolder & rules = drule::rules();

    string buffer;
    ModelReaderPtr(pl.GetReader(DRAWING_RULES_TXT_FILE)).ReadAsString(buffer);

    rules.LoadFromTextProto(buffer);

    ofstream s((path + DRAWING_RULES_BIN_FILE).c_str(), ios::out | ios::binary);
    rules.SaveToBinaryProto(buffer, s);
  }
#endif

  // Generating intermediate files
  if (FLAGS_preprocess_xml)
  {
    LOG(LINFO, ("Generating intermediate data ...."));
    if (!data::GenerateToFile(FLAGS_intermediate_data_path, FLAGS_use_light_nodes))
      return -1;
  }

  feature::GenerateInfo genInfo;
  genInfo.m_tmpDir = FLAGS_intermediate_data_path;

  // load classificator only if necessary
  if (FLAGS_make_coasts || FLAGS_generate_features || FLAGS_generate_geometry ||
      FLAGS_generate_index || FLAGS_generate_search_index ||
      FLAGS_calc_statistics || FLAGS_dump_types || FLAGS_dump_prefixes)
  {
    classificator::Load();
    classif().SortClassificator();
  }

  // Generate dat file
  if (FLAGS_generate_features || FLAGS_make_coasts)
  {
    LOG(LINFO, ("Generating final data ..."));

    if (FLAGS_split_by_polygons)
    {
      // do split by countries
      genInfo.m_datFilePrefix = path;
    }
    else
    {
      // Used for one country generation - append destination file.
      // FLAGS_output may be empty for FLAGS_make_coasts flag.
      genInfo.m_datFilePrefix = path + FLAGS_output;
    }

    genInfo.m_datFileSuffix = DATA_FILE_EXTENSION_TMP;

    genInfo.m_splitByPolygons = FLAGS_split_by_polygons;
    genInfo.m_createWorld = FLAGS_generate_world;
    genInfo.m_makeCoasts = FLAGS_make_coasts;
    genInfo.m_emitCoasts = FLAGS_emit_coasts;

    if (!feature::GenerateFeatures(genInfo, FLAGS_use_light_nodes))
      return -1;

    // without --spit_by_polygons, we have empty name country as result - assign it
    if (genInfo.m_bucketNames.size() == 1 && genInfo.m_bucketNames[0].empty())
      genInfo.m_bucketNames[0] = FLAGS_output;

    if (FLAGS_generate_world)
    {
      genInfo.m_bucketNames.push_back(WORLD_FILE_NAME);
      genInfo.m_bucketNames.push_back(WORLD_COASTS_FILE_NAME);
    }
  }
  else
  {
    if (!FLAGS_output.empty())
      genInfo.m_bucketNames.push_back(FLAGS_output);
  }

  // Enumerate over all dat files that were created.
  size_t const count = genInfo.m_bucketNames.size();
  for (size_t i = 0; i < count; ++i)
  {
    string const & country = genInfo.m_bucketNames[i];

    if (FLAGS_generate_geometry)
    {
      LOG(LINFO, ("Generating result features for file", country));

      int mapType = feature::DataHeader::country;
      if (country == WORLD_FILE_NAME)
        mapType = feature::DataHeader::world;
      if (country == WORLD_COASTS_FILE_NAME)
        mapType = feature::DataHeader::worldcoasts;

      if (!feature::GenerateFinalFeatures(path, country, mapType))
      {
        // If error - move to next bucket without index generation
        continue;
      }
    }

    string const datFile = path + country + DATA_FILE_EXTENSION;

    if (FLAGS_generate_index)
    {
      LOG(LINFO, ("Generating index for ", datFile));

      if (!indexer::BuildIndexFromDatFile(datFile, FLAGS_intermediate_data_path + country))
        LOG(LCRITICAL, ("Error generating index."));
    }

    if (FLAGS_generate_search_index)
    {
      LOG(LINFO, ("Generating search index for ", datFile));

      if (!indexer::BuildSearchIndexFromDatFile(country + DATA_FILE_EXTENSION, true))
        LOG(LCRITICAL, ("Error generating search index."));
    }
  }

  // Create http update list for countries and corresponding files
  if (FLAGS_generate_update)
  {
    LOG(LINFO, ("Updating countries file..."));
    update::UpdateCountries(path);
  }

  if (!FLAGS_generate_borders.empty())
  {
    if (!FLAGS_output.empty())
    {
      osm::GenerateBordersFromOsm(FLAGS_generate_borders,
                                  path + FLAGS_output + ".osm",
                                  path + FLAGS_output + ".borders");
    }
    else
    {
      LOG(LINFO, ("Please specify osm country borders file in 'output' command line parameter."));
    }
  }

  string const datFile = path + FLAGS_output + DATA_FILE_EXTENSION;

  if (FLAGS_calc_statistics)
  {
    LOG(LINFO, ("Calculating statistics for ", datFile));

    stats::FileContainerStatistic(datFile);

    stats::MapInfo info;
    stats::CalcStatistic(datFile, info);
    stats::PrintStatistic(info);
  }

  if (FLAGS_dump_types)
    feature::DumpTypes(datFile);

  if (FLAGS_dump_prefixes)
    feature::DumpPrefixes(datFile);

  if (FLAGS_dump_search_tokens)
    feature::DumpSearchTokens(datFile);

  if (FLAGS_unpack_mwm)
    UnpackMwm(datFile);

  if (!FLAGS_delete_section.empty())
    DeleteSection(datFile, FLAGS_delete_section);

  if (FLAGS_generate_packed_borders)
    borders::GeneratePackedBorders(path);

  return 0;
}
