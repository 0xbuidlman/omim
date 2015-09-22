#include "zip_creator.hpp"

#include "../../coding/file_name_utils.hpp"
#include "../../coding/internal/file_data.hpp"
#include "../../coding/constants.hpp"

#include "../../std/vector.hpp"
#include "../../std/ctime.hpp"
#include "../../std/algorithm.hpp"

#include "../../3party/zlib/contrib/minizip/zip.h"


namespace
{
  struct ZipHandle
  {
    zipFile m_zipFile;
    ZipHandle(string const & filePath)
    {
      m_zipFile = zipOpen(filePath.c_str(), 0);
    }
    ~ZipHandle()
    {
      if (m_zipFile)
        zipClose(m_zipFile, NULL);
    }
  };

  void CreateTMZip(tm_zip & res)
  {
    time_t rawtime;
    struct tm * timeinfo;
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    res.tm_sec = timeinfo->tm_sec;
    res.tm_min = timeinfo->tm_min;
    res.tm_hour = timeinfo->tm_hour;
    res.tm_mday = timeinfo->tm_mday;
    res.tm_mon = timeinfo->tm_mon;
    res.tm_year = timeinfo->tm_year;
  }
}

bool CreateZipFromPathDeflatedAndDefaultCompression(string const & filePath, string const & zipFilePath)
{
  ZipHandle zip(zipFilePath);
  if (!zip.m_zipFile)
    return false;

  // Special syntax to initialize struct with zeroes
  zip_fileinfo zipInfo = zip_fileinfo();
  CreateTMZip(zipInfo.tmz_date);
  string fileName = filePath;
  my::GetNameFromFullPath(fileName);
  if (zipOpenNewFileInZip(zip.m_zipFile, fileName.c_str(), &zipInfo,
                          NULL, 0, NULL, 0, "ZIP from MapsWithMe", Z_DEFLATED, Z_DEFAULT_COMPRESSION) < 0)
  {
    return false;
  }

  my::FileData f(filePath, my::FileData::OP_READ);

  size_t const bufSize = READ_FILE_BUFFER_SIZE;
  vector<char> buffer(bufSize);
  size_t const fileSize = f.Size();
  size_t currSize = 0;

  while (currSize < fileSize)
  {
    size_t const toRead = min(bufSize, fileSize - currSize);
    f.Read(currSize, &buffer[0], toRead);

    if (ZIP_OK != zipWriteInFileInZip(zip.m_zipFile, &buffer[0], toRead))
      return false;

    currSize += toRead;
  }

  return true;
}
