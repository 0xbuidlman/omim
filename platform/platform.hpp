#pragma once

#include "coding/reader.hpp"

#include "base/exception.hpp"

#include "std/string.hpp"
#include "std/vector.hpp"
#include "std/utility.hpp"
#include "std/function.hpp"
#include "std/bitset.hpp"

#include "defines.hpp"


DECLARE_EXCEPTION(FileAbsentException, RootException);
DECLARE_EXCEPTION(NotImplementedException, RootException);

class Platform
{
public:
  enum EError
  {
    ERR_OK = 0,
    ERR_UNKNOWN = 1
  };
  
protected:
  /// Usually read-only directory for application resources
  string m_resourcesDir;
  /// Optional resource search path
  string m_optionalDir;

  /// Writable directory to store downloaded map data
  /// @note on some systems it can point to external ejectable storage
  string m_writableDir;
  /// Temporary directory, can be cleaned up by the system
  string m_tmpDir;
  /// Writable directory to store persistent application data
  string m_settingsDir;

  /// Extended resource files.
  /// Used in Android only (downloaded zip files as a container).
  vector<string> m_extResFiles;
  /// Default search scope for resource files.
  /// Used in Android only and initialized according to the market type (Play, Amazon, Samsung).
  string m_androidDefResScope;

  /// Used in Android only to get corret GUI elements layout.
  bool m_isTablet;

  /// Internal function to get full path for input file.
  /// Uses m_writeableDir [w], m_resourcesDir [r], m_settingsDir [s].
  string ReadPathForFile(string const & file, string searchScope = string()) const;

  /// Hash some unique string into uniform format.
  static string HashUniqueID(string const & s);

public:
  Platform();

  static bool IsFileExistsByFullPath(string const & filePath);

  /// @return void
  void AddOptionalPath(string const &path) {m_optionalDir = path;}
  /// @return always the same writable dir for current user with slash at the end
  string WritableDir() const { return m_writableDir; }
  /// @return full path to file in user's writable directory
  string WritablePathForFile(string const & file) const { return WritableDir() + file; }
  /// @return full path to indexes directory for country file.
  /// Creates directory if it's not exists. Throw Writer::CreateDirException if can't create.
  string WritablePathForCountryIndexes(string const & mwmName) const;
  /// @return generate full path to index based on mwmName and index extension
  string GetIndexFileName(string const & mwmName, string const & extension) const;

  /// @return resource dir (on some platforms it's differ from Writable dir)
  string ResourcesDir() const { return m_resourcesDir; }

  /// Creates directory at filesystem
  EError MkDir(string const & dirName) const;

  /// @TODO create join method for string concatenation

  /// @return path for directory with temporary files with slash at the end
  string TmpDir() const { return m_tmpDir; }
  /// @return full path to file in the temporary directory
  string TmpPathForFile(string const & file) const { return TmpDir() + file; }

  /// @return path for directory in the persistent memory, can be the same
  /// as WritableDir, but on some platforms it's different
  string SettingsDir() const { return m_settingsDir; }
  /// @return full path to file in the settings directory
  string SettingsPathForFile(string const & file) const { return SettingsDir() + file; }

  /// @return reader for file decriptor.
  /// @throws FileAbsentException
  /// @param[in] file name or full path which we want to read, don't forget to free memory or wrap it to ReaderPtr
  /// @param[in] searchScope looks for file in dirs in given order: \n
  ///  [w]ritable, [r]esources, [s]ettings, by [f]ull path, [e]xternal resources,
  ModelReader * GetReader(string const & file, string const & searchScope = string()) const;

  /// @name File operations
  //@{
  typedef vector<string> FilesList;
  /// Retrieves files list contained in given directory
  /// @param directory directory path with slash at the end
  //@{
  /// @param ext files extension to find, like ".mwm".
  static void GetFilesByExt(string const & directory, string const & ext, FilesList & outFiles);
  static void GetFilesByRegExp(string const & directory, string const & regexp, FilesList & outFiles);
  //@}

  /// @return false if file is not exist
  /// @note Check files in Writable dir first, and in ReadDir if not exist in Writable dir
  bool GetFileSizeByName(string const & fileName, uint64_t & size) const;
  /// @return false if file is not exist
  /// @note Try do not use in client production code
  static bool GetFileSizeByFullPath(string const & filePath, uint64_t & size);
  //@}

  /// Used to check available free storage space for downloading.
  enum TStorageStatus
  {
    STORAGE_OK = 0,
    STORAGE_DISCONNECTED,
    NOT_ENOUGH_SPACE
  };
  TStorageStatus GetWritableStorageStatus(uint64_t neededSize) const;

  /// @name Functions for concurrent tasks.
  //@{
  typedef function<void()> TFunctor;
  void RunOnGuiThread(TFunctor const & fn);
  enum Priority
  {
    EPriorityBackground,
    EPriorityLow,
    EPriorityDefault,
    EPriorityHigh
  };
  void RunAsync(TFunctor const & fn, Priority p = EPriorityDefault);
  //@}

  int CpuCores() const;

  void GetFontNames(FilesList & res) const;

  int VideoMemoryLimit() const;

  int PreCachingDepth() const;

  string DeviceName() const;

  string UniqueClientId() const;

  /// @return url for clients to download maps
  //@{
  string MetaServerUrl() const;
  string ResourcesMetaServerUrl() const;
  //@}

  /// @return JSON-encoded list of urls if metaserver is unreachable
  string DefaultUrlsJSON() const;

  bool IsTablet() const { return m_isTablet; }

  /// @return information about kinds of memory which are relevant for a platform.
  /// This methid is implemented for iOS and Android only.
  string GetMemoryInfo() const;

private:
  void GetSystemFontNames(FilesList & res) const;
};

extern Platform & GetPlatform();
