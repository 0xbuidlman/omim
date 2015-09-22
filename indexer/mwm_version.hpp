#pragma once

#include "../std/stdint.hpp"

class FilesContainerR;
class ReaderSrc;
class Writer;

namespace version
{
enum Format
{
  unknownFormat = -1,
  v1 = 0,  // April 2011
  v2,      // November 2011 (store type index, instead of raw type in mwm)
  v3,      // March 2013 (store type index, instead of raw type in search data)
  lastFormat = v3
};

struct MwmVersion {
  MwmVersion();

  Format format;
  uint32_t timestamp;
};

/// Writes latest format and current timestamp to the writer.
void WriteVersion(Writer & w);

/// Reads mwm version from src.
void ReadVersion(ReaderSrc & src, MwmVersion & version);

/// \return True when version was successfully parsed from container,
///         otherwise returns false. In the latter case version is
///         unchanged.
bool ReadVersion(FilesContainerR const & container, MwmVersion & version);
}  // namespace version
