#pragma once
#include "dd_vector.hpp"
#include "polymorph_reader.hpp"
#include "../std/function.hpp"
#include "../std/scoped_ptr.hpp"
#include "../std/string.hpp"
#include "../base/base.hpp"
#include "../base/exception.hpp"

class Reader;

class BlobStorage
{
public:
  DECLARE_EXCEPTION(OpenException, RootException);

  // Takes ownership of pReader and deletes it, even if exception is thrown.
  BlobStorage(Reader const * pReader,
              function<void (char const *, size_t, char *, size_t)> decompressor);
  ~BlobStorage();

  // Get blob by its number, starting from 0.
  void GetBlob(uint32_t i, string & blob) const;

  // Returns the number of blobs.
  uint32_t Size() const;

private:
  void Init();

  uint32_t GetChunkFromBI(uint32_t blobInfo) const;
  uint32_t GetOffsetFromBI(uint32_t blobInfo) const;

  uint32_t m_bitsInChunkSize;
  static uint32_t const START_OFFSET = 4;

  scoped_ptr<Reader const> m_pReader;
  function<void (char const *, size_t, char *, size_t)> m_decompressor;

  DDVector<uint32_t, PolymorphReader> m_blobInfo;
  DDVector<uint32_t, PolymorphReader> m_chunkOffset;
};
