#include "blob_storage.hpp"
#include "reader.hpp"

// File Format:
// Blobs are grouped together in chunks and then chunks are compressed.
// nb - number of blobs
// nc - number of chunks
//
// [3| Header = "Blb"]
// [1| logMaxChunkSize]
// [*| Chunk 0    ] [*| Chunk 1    ] ... [*| Chunk nc-1]
// [4| Chunk 1 pos] [4| Chunk 2 pos] ... [4| Pos after the last chunk]
// [4| Blob info 0] [4| Blob info 1] ... [4| Blob info nb-1]
// [4| nb]
//
//
// Chunk Format:
// [*| Chunk data]
// [4| Uncompressed chunk size]
//
// Blob Info Format:
// [       Chunk number      ]  [Offset in uncompressed chunk]
// | 32 - BITS_IN_CHUNK_SIZE |  |     BITS_IN_CHUNK_SIZE     |


BlobStorage::BlobStorage(Reader const * pReader,
                         DecompressorType const & decompressor) :
  m_pReader(pReader), m_decompressor(decompressor)
{
  Init();
}

BlobStorage::~BlobStorage()
{
}

void BlobStorage::Init()
{
  string header(3, ' ');
  ReadFromPos(*m_pReader, 0, &header[0], 3);
  if (header != "Blb")
    MYTHROW(BlobStorage::OpenException, (header));
  m_bitsInChunkSize = ReadPrimitiveFromPos<uint8_t>(*m_pReader, 3);

  uint64_t const fileSize = m_pReader->Size();
  uint32_t const blobCount = ReadPrimitiveFromPos<uint32_t>(*m_pReader, fileSize - 4);
  m_blobInfo.Init(PolymorphReader(m_pReader->CreateSubReader(
                                    fileSize - 4 - 4 * blobCount,
                                    4 * blobCount)));
  uint32_t const chunkCount =
      (blobCount > 0 ? (m_blobInfo[blobCount - 1] >> m_bitsInChunkSize) + 1 : 0);
  m_chunkOffset.Init(PolymorphReader(m_pReader->CreateSubReader(
                                       fileSize - 4 - 4 * blobCount - 4 * chunkCount,
                                       4 * chunkCount)));
}

uint32_t BlobStorage::Size() const
{
  return m_blobInfo.size();
}

uint32_t BlobStorage::GetChunkFromBI(uint32_t blobInfo) const
{
  return blobInfo >> m_bitsInChunkSize;
}

uint32_t BlobStorage::GetOffsetFromBI(uint32_t blobInfo) const
{
  return blobInfo & ((1 << m_bitsInChunkSize) - 1);
}

void BlobStorage::GetBlob(uint32_t i, string & blob) const
{
  ASSERT_LESS(i, Size(), ());
  uint32_t const blobInfo = m_blobInfo[i];
  uint32_t const chunk = GetChunkFromBI(blobInfo);
  uint32_t const chunkBeg = (chunk == 0 ? 0 : m_chunkOffset[chunk - 1]);
  uint32_t const chunkEnd = m_chunkOffset[chunk];
  vector<char> compressedData(chunkEnd - chunkBeg);
  ASSERT_GREATER(compressedData.size(), 4, ());
  m_pReader->Read(START_OFFSET + chunkBeg, &compressedData[0], compressedData.size());
  uint32_t const decompressedSize = ReadPrimitiveFromPos<uint32_t>(
      MemReader(&compressedData[0], compressedData.size()), compressedData.size() - 4);

  vector<char> data(decompressedSize);
  m_decompressor(&compressedData[0], compressedData.size() - 4, &data[0], data.size());

  uint32_t const blobOffset = GetOffsetFromBI(blobInfo);
  if (i != m_blobInfo.size() - 1 && chunk == GetChunkFromBI(m_blobInfo[i+1]))
    blob.assign(data.begin() + blobOffset, data.begin() + GetOffsetFromBI(m_blobInfo[i+1]));
  else
    blob.assign(data.begin() + blobOffset, data.end());
}
