#include "../base/SRC_FIRST.hpp"

#include "feature_loader_base.hpp"
#include "feature_loader.hpp"
#include "feature_impl.hpp"

#include "old/feature_loader_101.hpp"

#include "../defines.hpp"

#include "../coding/byte_stream.hpp"


namespace feature
{

////////////////////////////////////////////////////////////////////////////////////////////
// SharedLoadInfo implementation.
////////////////////////////////////////////////////////////////////////////////////////////

SharedLoadInfo::SharedLoadInfo(FilesContainerR const & cont, DataHeader const & header)
  : m_cont(cont), m_header(header)
{
  CreateLoader();
}

SharedLoadInfo::~SharedLoadInfo()
{
  delete m_pLoader;
}

SharedLoadInfo::ReaderT SharedLoadInfo::GetDataReader() const
{
  return m_cont.GetReader(DATA_FILE_TAG);
}

SharedLoadInfo::ReaderT SharedLoadInfo::GetGeometryReader(int ind) const
{
  return m_cont.GetReader(GetTagForIndex(GEOMETRY_FILE_TAG, ind));
}

SharedLoadInfo::ReaderT SharedLoadInfo::GetTrianglesReader(int ind) const
{
  return m_cont.GetReader(GetTagForIndex(TRIANGLE_FILE_TAG, ind));
}

void SharedLoadInfo::CreateLoader()
{
  switch (m_header.GetVersion())
  {
  case DataHeader::v1:
    m_pLoader = new old_101::feature::LoaderImpl(*this);
    break;

  default:
    m_pLoader = new LoaderCurrent(*this);
    break;
  }
}


////////////////////////////////////////////////////////////////////////////////////////////
// LoaderBase implementation.
////////////////////////////////////////////////////////////////////////////////////////////

LoaderBase::LoaderBase(SharedLoadInfo const & info)
  : m_Info(info), m_pF(0), m_Data(0)
{
}

void LoaderBase::Init(BufferT data)
{
  m_Data = data;
  m_pF = 0;

  m_CommonOffset = m_Header2Offset = 0;
  m_ptsSimpMask = 0;

  m_ptsOffsets.clear();
  m_trgOffsets.clear();
}

uint32_t LoaderBase::CalcOffset(ArrayByteSource const & source) const
{
  return static_cast<uint32_t>(source.PtrC() - DataPtr());
}

void LoaderBase::ReadOffsets(ArrayByteSource & src, uint8_t mask, offsets_t & offsets) const
{
  ASSERT ( offsets.empty(), () );
  ASSERT_GREATER ( mask, 0, () );

  offsets.resize(m_Info.GetScalesCount(), s_InvalidOffset);
  size_t ind = 0;

  while (mask > 0)
  {
    if (mask & 0x01)
      offsets[ind] = ReadVarUint<uint32_t>(src);

    ++ind;
    mask = mask >> 1;
  }
}

}
