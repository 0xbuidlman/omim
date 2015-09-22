#include "platform.hpp"

#include "../base/scope_guard.hpp"
#include "../base/logging.hpp"

#include "../coding/file_writer.hpp"

#include "../std/windows.hpp"
#include "../std/bind.hpp"

#include <shlobj.h>

static bool GetUserWritableDir(string & outDir)
{
  char pathBuf[MAX_PATH] = {0};
  if (SUCCEEDED(::SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, pathBuf)))
  {
    outDir = pathBuf;
    ::CreateDirectoryA(outDir.c_str(), NULL);
    outDir += "\\MapsWithMe\\";
    ::CreateDirectoryA(outDir.c_str(), NULL);
    return true;
  }
  return false;
}

/// @return Full path to the executable file
static bool GetPathToBinary(string & outPath)
{
  // get path to executable
  char pathBuf[MAX_PATH] = {0};
  if (0 < ::GetModuleFileNameA(NULL, pathBuf, MAX_PATH))
  {
    outPath = pathBuf;
    return true;
  }
  return false;
}

Platform::Platform()
{
  string path;
  CHECK(GetPathToBinary(path), ("Can't get path to binary"));

  // resources path:
  // 1. try to use data folder in the same path as executable
  // 2. if not found, try to use ..\..\..\data (for development only)
  path.erase(path.find_last_of('\\'));
  if (IsFileExistsByFullPath(path + "\\data\\"))
    m_resourcesDir = path + "\\data\\";
  else
  {
#ifndef OMIM_PRODUCTION
    path.erase(path.find_last_of('\\'));
    path.erase(path.find_last_of('\\'));
    if (IsFileExistsByFullPath(path + "\\data\\"))
      m_resourcesDir = path + "\\data\\";
#else
    CHECK(false, ("Can't find resources directory"));
#endif
  }

  // writable path:
  // 1. the same as resources if we have write access to this folder
  // 2. otherwise, use system-specific folder
  try
  {
    FileWriter tmpfile(m_resourcesDir + "mapswithmetmptestfile");
    tmpfile.Write("Hi from Alex!", 13);
    m_writableDir = m_resourcesDir;
  }
  catch (RootException const &)
  {
    CHECK(GetUserWritableDir(m_writableDir), ("Can't get writable directory"));
  }
  FileWriter::DeleteFileX(m_resourcesDir + "mapswithmetmptestfile");

  m_settingsDir = m_writableDir;
  char pathBuf[MAX_PATH] = {0};
  GetTempPathA(MAX_PATH, pathBuf);
  m_tmpDir = pathBuf;

  LOG(LDEBUG, ("Resources Directory:", m_resourcesDir));
  LOG(LDEBUG, ("Writable Directory:", m_writableDir));
  LOG(LDEBUG, ("Tmp Directory:", m_tmpDir));
  LOG(LDEBUG, ("Settings Directory:", m_settingsDir));
}

bool Platform::IsFileExistsByFullPath(string const & filePath)
{
  return ::GetFileAttributesA(filePath.c_str()) != INVALID_FILE_ATTRIBUTES;
}

int Platform::CpuCores() const
{
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  DWORD numCPU = sysinfo.dwNumberOfProcessors;
  if (numCPU >= 1)
    return static_cast<int>(numCPU);
  return 1;
}

string Platform::UniqueClientId() const
{
  return "@TODO";
}

void Platform::RunOnGuiThread(TFunctor const & fn)
{
  /// @todo
  fn();
}

void Platform::RunAsync(TFunctor const & fn, Priority p)
{
  /// @todo
  fn();
}

Platform::TStorageStatus Platform::GetWritableStorageStatus(uint64_t neededSize) const
{
  ULARGE_INTEGER freeSpace;
  if (0 == ::GetDiskFreeSpaceExA(m_writableDir.c_str(), &freeSpace, NULL, NULL))
  {
    LOG(LWARNING, ("GetDiskFreeSpaceEx failed with error", GetLastError()));
    return STORAGE_DISCONNECTED;
  }

  if (freeSpace.u.LowPart + (freeSpace.u.HighPart << 32) < neededSize)
    return NOT_ENOUGH_SPACE;

  return STORAGE_OK;
}

bool Platform::GetFileSizeByFullPath(string const & filePath, uint64_t & size)
{
  HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile != INVALID_HANDLE_VALUE)
  {
    MY_SCOPE_GUARD(autoClose, bind(&CloseHandle, hFile));
    LARGE_INTEGER fileSize;
    if (0 != GetFileSizeEx(hFile, &fileSize))
    {
      size = fileSize.QuadPart;
      return true;
    }
  }
  return false;
}
