#include "index.hpp"
#include "data_header.hpp"

#include "../platform/platform.hpp"

#include "../coding/internal/file_data.hpp"


MwmValue::MwmValue(string const & name)
  : m_cont(GetPlatform().GetReader(name)), m_name(name)
{
  m_factory.Load(m_cont);
}

int Index::GetInfo(string const & name, MwmInfo & info) const
{
  MwmValue value(name);

  feature::DataHeader const & h = value.GetHeader();
  info.m_limitRect = h.GetBounds();

  pair<int, int> const scaleR = h.GetScaleRange();
  info.m_minScale = static_cast<uint8_t>(scaleR.first);
  info.m_maxScale = static_cast<uint8_t>(scaleR.second);

  return h.GetVersion();
}

MwmValue * Index::CreateValue(string const & name) const
{
  return new MwmValue(name);
}

Index::Index()
{
}

Index::~Index()
{
  Cleanup();
}

namespace
{
  void DeleteMapFiles(string const & path, bool deleteReady)
  {
    (void)my::DeleteFileX(path);
    (void)my::DeleteFileX(path + RESUME_FILE_EXTENSION);
    (void)my::DeleteFileX(path + DOWNLOADING_FILE_EXTENSION);

    if (deleteReady)
      (void)my::DeleteFileX(path + READY_FILE_EXTENSION);
  }

  string GetFullPath(string const & fileName)
  {
    return GetPlatform().WritablePathForFile(fileName);
  }

  void ReplaceFileWithReady(string const & fileName)
  {
    string const path = GetFullPath(fileName);
    DeleteMapFiles(path, false);
    CHECK ( my::RenameFileX(path + READY_FILE_EXTENSION, path), (path) );
  }
}

bool Index::DeleteMap(string const & fileName)
{
  threads::MutexGuard mutexGuard(m_lock);
  UNUSED_VALUE(mutexGuard);

  if (!RemoveImpl(fileName))
    return false;

  DeleteMapFiles(GetFullPath(fileName), true);
  return true;
}

bool Index::UpdateMap(string const & fileName, m2::RectD & rect)
{
  threads::MutexGuard mutexGuard(m_lock);
  UNUSED_VALUE(mutexGuard);

  MwmId const id = GetIdByName(fileName);
  if (id != INVALID_MWM_ID)
  {
    m_info[id].m_status = MwmInfo::STATUS_UPDATE;
    return false;
  }

  ReplaceFileWithReady(fileName);

  (void)AddImpl(fileName, rect);
  return true;
}

void Index::UpdateMwmInfo(MwmId id)
{
  switch (m_info[id].m_status)
  {
  case MwmInfo::STATUS_TO_REMOVE:
    if (m_info[id].m_lockCount == 0)
    {
      DeleteMapFiles(m_name[id], true);

      CHECK(RemoveImpl(id), ());
    }
    break;

  case MwmInfo::STATUS_UPDATE:
    if (m_info[id].m_lockCount == 0)
    {
      ClearCache(id);

      ReplaceFileWithReady(m_name[id]);

      m_info[id].m_status = MwmInfo::STATUS_ACTIVE;
    }
    break;
  }
}

Index::FeaturesLoaderGuard::FeaturesLoaderGuard(Index const & parent, MwmId id)
  : m_lock(parent, id),
    /// @note This guard is suitable when mwm is loaded
    m_vector(m_lock.GetValue()->m_cont, m_lock.GetValue()->GetHeader())
{
}

void Index::FeaturesLoaderGuard::GetFeature(uint32_t offset, FeatureType & ft)
{
  m_vector.Get(offset, ft);
}
