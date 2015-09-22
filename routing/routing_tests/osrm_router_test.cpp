#include "../../testing/testing.hpp"

#include "../osrm_router.hpp"

#include "../../indexer/features_offsets_table.hpp"
#include "../../indexer/mercator.hpp"

#include "../../coding/file_writer.hpp"

#include "../../platform/platform.hpp"

#include "../../defines.hpp"

#include "../../base/scope_guard.hpp"

#include "../../std/bind.hpp"
#include "../../std/string.hpp"
#include "../../std/unique_ptr.hpp"
#include "../../std/vector.hpp"

using namespace routing;

namespace
{

typedef vector<OsrmFtSegMappingBuilder::FtSegVectorT> InputDataT;
typedef vector< vector<OsrmNodeIdT> > NodeIdDataT;
typedef vector< pair<size_t, size_t> > RangeDataT;
typedef OsrmMappingTypes::FtSeg SegT;

void TestNodeId(OsrmFtSegMapping const & mapping, NodeIdDataT const & test)
{
  for (OsrmNodeIdT nodeId = 0; nodeId < test.size(); ++nodeId)
  {
    for (auto idx : test[nodeId])
      TEST_EQUAL(nodeId, mapping.GetNodeId(idx), ());
  }
}

void TestSegmentRange(OsrmFtSegMapping const & mapping, RangeDataT const & test)
{
  for (OsrmNodeIdT nodeId = 0; nodeId < test.size(); ++nodeId)
  {
    // Input test range is { start, count } but we should pass [start, end).
    auto const & r = test[nodeId];
    TEST_EQUAL(mapping.GetSegmentsRange(nodeId), make_pair(r.first, r.first + r.second), ());
  }
}

void TestMapping(InputDataT const & data,
                 NodeIdDataT const & nodeIds,
                 RangeDataT const & ranges)
{
  static char const ftSegsPath[] = "test1.tmp";
  string const featuresOffsetsTablePath =
      GetPlatform().GetIndexFileName(ftSegsPath, FEATURES_OFFSETS_TABLE_FILE_EXT);
  MY_SCOPE_GUARD(ftSegsFileDeleter, bind(FileWriter::DeleteFileX, ftSegsPath));
  MY_SCOPE_GUARD(featuresOffsetsTableFileDeleter,
                 bind(FileWriter::DeleteFileX, featuresOffsetsTablePath));

  {
    // Prepare fake features offsets table for input data, just push all
    // segments feature ids as offsets.
    feature::FeaturesOffsetsTable::Builder tableBuilder;
    for (auto const & segVector : data)
    {
      for (auto const & seg : segVector)
        tableBuilder.PushOffset(seg.m_fid);
    }
    unique_ptr<feature::FeaturesOffsetsTable> table =
        feature::FeaturesOffsetsTable::Build(tableBuilder);
    table->Save(featuresOffsetsTablePath);
  }

  OsrmFtSegMappingBuilder builder;
  for (OsrmNodeIdT nodeId = 0; nodeId < data.size(); ++nodeId)
    builder.Append(nodeId, data[nodeId]);

  TestNodeId(builder, nodeIds);
  TestSegmentRange(builder, ranges);

  {
    FilesContainerW w(ftSegsPath);
    builder.Save(w);
  }

  {
    FilesMappingContainer cont(ftSegsPath);
    OsrmFtSegMapping mapping;
    mapping.Load(cont);

    TestNodeId(mapping, nodeIds);
    TestSegmentRange(mapping, ranges);

    for (size_t i = 0; i < mapping.GetSegmentsCount(); ++i)
    {
      OsrmNodeIdT const node = mapping.GetNodeId(i);
      size_t count = 0;
      mapping.ForEachFtSeg(node, [&] (OsrmMappingTypes::FtSeg const & s)
      {
        TEST_EQUAL(s, data[node][count++], ());
      });
      TEST_EQUAL(count, data[node].size(), ());
    }
  }
}

bool TestFtSeg(SegT const & s)
{
  return (SegT(s.Store()) == s);
}

}

UNIT_TEST(FtSeg_Smoke)
{
  SegT arr[] = {
    { 5, 1, 2 },
    { 666, 0, 17 },
  };

  for (size_t i = 0; i < ARRAY_SIZE(arr); ++i)
    TEST(TestFtSeg(arr[i]), (arr[i].Store()));
}

UNIT_TEST(OsrmFtSegMappingBuilder_Smoke)
{
  {
    InputDataT data =
    {
      { {0, 0, 1} },
      { {1, 0, 1} },
      { {2, 0, 1}, {3, 0, 1} },
      { {4, 0, 1} },
      { {5, 0, 1}, {6, 0, 1}, {7, 0, 1} },
      { {8, 0, 1}, {9, 0, 1}, {10, 0, 1}, {11, 0, 1} },
      { {12, 0, 1} }
    };

    NodeIdDataT nodeIds =
    {
      { 0 },
      { 1 },
      { 2, 3 },
      { 4 },
      { 5, 6, 7 },
      { 8, 9, 10, 11 },
      { 12 }
    };

    RangeDataT ranges =
    {
      { 0, 1 },
      { 1, 1 },
      { 2, 2 },
      { 4, 1 },
      { 5, 3 },
      { 8, 4 },
      { 12, 1 },
    };

    TestMapping(data, nodeIds, ranges);
  }


  {
    InputDataT data =
    {
      { {0, 0, 1} },
      { {1, 0, 1} },
      { {2, 0, 1} },
      { {3, 0, 1} },
      { {4, 0, 1} },
      { {5, 0, 1}, {6, 0, 1} },
      { {7, 0, 1} },
      { {8, 0, 1}, {9, 0, 1}, {10, 0, 1} },
      { {11, 0, 1}, {12, 0, 1}, {13, 0, 1} }
    };

    NodeIdDataT nodeIds =
    {
      { 0 },
      { 1 },
      { 2 },
      { 3 },
      { 4 },
      { 5, 6 },
      { 7 },
      { 8, 9, 10 },
      { 11, 12, 13 }
    };

    RangeDataT ranges =
    {
      { 0, 1 },
      { 1, 1 },
      { 2, 1 },
      { 3, 1 },
      { 4, 1 },
      { 5, 2 },
      { 7, 1 },
      { 8, 3 },
      { 11, 3 },
    };

    TestMapping(data, nodeIds, ranges);
  }


  {
    InputDataT data =
    {
      { {0, 0, 1}, {1, 2, 3} },
      { },
      { {3, 6, 7} },
      { {4, 8, 9}, {5, 10, 11} },
    };

    NodeIdDataT nodeIds =
    {
      { 0, 1 },
      { },
      { 3 },
      { 4, 5 },
    };

    RangeDataT ranges =
    {
      { 0, 2 },
      { 2, 1 },
      { 3, 1 },
      { 4, 2 },
    };

    TestMapping(data, nodeIds, ranges);
  }
}
