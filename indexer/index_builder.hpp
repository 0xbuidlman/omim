#pragma once

#include "scale_index_builder.hpp"

namespace indexer
{
  template <class FeaturesVectorT, typename WriterT>
  void BuildIndex(uint32_t bucketsCount,
                  FeaturesVectorT const & featuresVector,
                  WriterT & writer,
                  string const & tmpFilePrefix)
  {
    LOG(LINFO, ("Building scale index."));
    uint64_t indexSize;
    {
      SubWriter<WriterT> subWriter(writer);
      IndexScales(bucketsCount, featuresVector, subWriter, tmpFilePrefix);
      indexSize = subWriter.Size();
    }
    LOG(LINFO, ("Built scale index. Size =", indexSize));
  }

  // doesn't throw exceptions
  bool BuildIndexFromDatFile(string const & datFile, string const & tmpFile);
}
