#include "../../testing/testing.hpp"

#include "../index.hpp"
#include "../index_builder.hpp"
#include "../classificator_loader.hpp"
#include "../features_vector.hpp"

#include "../../defines.hpp"

#include "../../platform/platform.hpp"

#include "../../coding/file_container.hpp"

#include "../../base/stl_add.hpp"


UNIT_TEST(BuildIndexTest)
{
  Platform & p = GetPlatform();
  classificator::Read(p.GetReader("drawing_rules.bin"),
                      p.GetReader("classificator.txt"),
                      p.GetReader("visibility.txt"),
                      p.GetReader("types.txt"));

  FilesContainerR originalContainer(p.GetReader("minsk-pass" DATA_FILE_EXTENSION));

  // Build index.
  vector<char> serialIndex;
  {
    feature::DataHeader header;
    header.Load(originalContainer.GetReader(HEADER_FILE_TAG));

    FeaturesVector featuresVector(originalContainer, header);

    MemWriter<vector<char> > serialWriter(serialIndex);
    indexer::BuildIndex(ScaleIndexBase::NUM_BUCKETS, featuresVector, serialWriter,
                        "build_index_test");
  }

  // Create a new mwm file.
  string const fileName = "build_index_test" DATA_FILE_EXTENSION;
  string const filePath = p.WritablePathForFile(fileName);
  FileWriter::DeleteFileX(filePath);

  // Copy original mwm file and replace index in it.
  {
    FilesContainerW containerWriter(filePath);
    vector<string> tags;
    originalContainer.ForEachTag(MakeBackInsertFunctor(tags));
    for (size_t i = 0; i < tags.size(); ++i)
    {
      if (tags[i] != INDEX_FILE_TAG)
        containerWriter.Append(originalContainer.GetReader(tags[i]), tags[i]);
    }
    containerWriter.Append(serialIndex, INDEX_FILE_TAG);
  }

  {
    // Check that index actually works.
    Index<ModelReaderPtr>::Type index;
    index.Add(fileName);

    // Make sure that index is actually parsed.
    index.ForEachInScale(NoopFunctor(), 15);
  }

  // Clean after the test.
  FileWriter::DeleteFileX(filePath);
}
