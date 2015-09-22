#include "platform.hpp"
#include "platform_unix_impl.hpp"

#include "../base/logging.hpp"
#include "../base/regexp.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include "../std/algorithm.hpp"

#if defined(OMIM_OS_MAC) || defined(OMIM_OS_IPHONE)
  #include <sys/mount.h>
#else
  #include <sys/vfs.h>
#endif

void Platform::GetSystemFontNames(FilesList & res) const
{
#if defined(OMIM_OS_MAC) || defined(OMIM_OS_IPHONE)
#else
  char const * fontsWhitelist[] = {
    "Roboto-Regular.ttf",
    "DroidSansFallback.ttf",
    "DroidSans.ttf",
    "DroidSansArabic.ttf",
    "DroidSansSemc.ttf",
    "DroidSansSemcCJK.ttf",
    "DroidNaskh-Regular.ttf",
    "Lohit-Bengali.ttf",
    "Lohit-Devanagari.ttf",
    "Lohit-Tamil.ttf",
    "DroidSansThai.ttf",
    "DroidSansArmenian.ttf",
    "DroidSansEthiopic-Regular.ttf",
    "DroidSansGeorgian.ttf",
    "DroidSansHebrew-Regular.ttf",
    "DroidSansHebrew.ttf",
    "DroidSansJapanese.ttf",
    "LTe50872.ttf",
    "LTe50259.ttf",
    "DevanagariOTS.ttf",
    "DejaVuSans.ttf",
    "arial.ttf"
  };

  char const * systemFontsPath[] = {
    "/system/fonts/",
    "/usr/share/fonts/truetype/roboto/",
    "/usr/share/fonts/truetype/droid/",
    "/usr/share/fonts/truetype/ttf-dejavu/"
  };
  
  const uint64_t fontSizeBlacklist[] = {
    183560,   // Samsung Duos DroidSans
    7140172,  // Serif font without Emoji
    14416824  // Serif font with Emoji
  };

  uint64_t fileSize = 0;

  for (size_t i = 0; i < ARRAY_SIZE(fontsWhitelist); ++i)
  {
    for (size_t j = 0; j < ARRAY_SIZE(systemFontsPath); ++j)
    {
      string const path = string(systemFontsPath[j]) + fontsWhitelist[i];
      if (IsFileExistsByFullPath(path))
      {
        if (GetFileSizeByName(path, fileSize))
        {
          uint64_t const * end = fontSizeBlacklist + ARRAY_SIZE(fontSizeBlacklist);
          if (find(fontSizeBlacklist, end, fileSize) == end)
          {
            res.push_back(path);
            LOG(LINFO, ("Found usable system font", path, "with file size", fileSize));
          }
        }
      }
    }
  }

  // Ignoring system fonts if broken Samsung Duos font detected
  if (GetPlatform().GetFileSizeByName("/system/fonts/DroidSans.ttf", fileSize))
  {
    if (fileSize == 183560)
    {
      res.clear();
      LOG(LINFO, ("Ignoring system fonts"));
    }
  }
#endif
}

bool Platform::IsFileExistsByFullPath(string const & filePath)
{
  struct stat s;
  return stat(filePath.c_str(), &s) == 0;
}

bool Platform::GetFileSizeByFullPath(string const & filePath, uint64_t & size)
{
  struct stat s;
  if (stat(filePath.c_str(), &s) == 0)
  {
    size = s.st_size;
    return true;
  }
  else return false;
}

Platform::TStorageStatus Platform::GetWritableStorageStatus(uint64_t neededSize) const
{
  struct statfs st;
  int const ret = statfs(m_writableDir.c_str(), &st);

  LOG(LDEBUG, ("statfs return = ", ret,
               "; block size = ", st.f_bsize,
               "; blocks available = ", st.f_bavail));

  if (ret != 0)
    return STORAGE_DISCONNECTED;

  /// @todo May be add additional storage space.
  if (st.f_bsize * st.f_bavail < neededSize)
    return NOT_ENOUGH_SPACE;

  return STORAGE_OK;
}

namespace pl
{

void EnumerateFilesByRegExp(string const & directory, string const & regexp,
                            vector<string> & res)
{
  DIR * dir;
  struct dirent * entry;
  if ((dir = opendir(directory.c_str())) == NULL)
    return;

  regexp::RegExpT exp;
  regexp::Create(regexp, exp);

  while ((entry = readdir(dir)) != 0)
  {
    string const name(entry->d_name);
    if (regexp::IsExist(name, exp))
      res.push_back(name);
  }

  closedir(dir);
}

}
