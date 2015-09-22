#pragma once

#include "indexer/data_header.hpp"
#include "indexer/features_vector.hpp"

#include "defines.hpp"

#include "coding/file_reader.hpp"
#include "coding/file_container.hpp"

#include "std/bind.hpp"


namespace feature
{
  template <class ToDo>
  void ForEachFromDat(ModelReaderPtr reader, ToDo & toDo)
  {
    FilesContainerR container(reader);

    DataHeader header;
    header.Load(container.GetReader(HEADER_FILE_TAG));

    FeaturesVector featureSource(container, header);
    featureSource.ForEachOffset(bind<void>(ref(toDo), _1, _2));
  }

  template <class ToDo>
  void ForEachFromDat(string const & fPath, ToDo & toDo)
  {
    ForEachFromDat(new FileReader(fPath), toDo);
  }
}
