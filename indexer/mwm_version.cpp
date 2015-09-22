#include "mwm_version.hpp"
#include "data_header.hpp"

#include "../coding/file_container.hpp"
#include "../coding/reader_wrapper.hpp"
#include "../coding/varint.hpp"
#include "../coding/writer.hpp"

#include "../base/logging.hpp"
#include "../base/timer.hpp"

#include "../defines.hpp"

#include "../std/ctime.hpp"

namespace version
{
namespace
{
typedef feature::DataHeader FHeaderT;

char const MWM_PROLOG[] = "MWM";

template <class TSource>
void ReadVersionT(TSource & src, MwmVersion & version)
{
  size_t const prologSize = ARRAY_SIZE(MWM_PROLOG);
  char prolog[prologSize];
  src.Read(prolog, prologSize);

  if (strcmp(prolog, MWM_PROLOG) != 0)
  {
    version.format = v2;
    version.timestamp =
        my::GenerateTimestamp(2011 - 1900 /* number of years since 1900 */,
                              10 /* number of month since January */, 1 /* month day */);
    return;
  }

  uint32_t formatIndex = ReadVarUint<uint32_t>(src);
  if (formatIndex > lastFormat)
  {
    LOG(LERROR, ("Unknown file format index:", formatIndex));
    formatIndex = lastFormat;
  }
  version.format = static_cast<Format>(formatIndex);
  version.timestamp = ReadVarUint<uint32_t>(src);
}
}  // namespace

MwmVersion::MwmVersion() : format(unknownFormat), timestamp(0) {}

void WriteVersion(Writer & w)
{
  w.Write(MWM_PROLOG, ARRAY_SIZE(MWM_PROLOG));

  // write inner data version
  WriteVarUint(w, static_cast<uint32_t>(lastFormat));

  // static is used for equal time stamp for all "mwm" files in one generation process
  static uint32_t generatorStartTime = my::TodayAsYYMMDD();
  WriteVarUint(w, generatorStartTime);
}

void ReadVersion(ReaderSrc & src, MwmVersion & version)
{
  ReadVersionT(src, version);
}

bool ReadVersion(FilesContainerR const & container, MwmVersion & version)
{
  if (!container.IsExist(VERSION_FILE_TAG))
    return false;
  ModelReaderPtr versionReader = container.GetReader(VERSION_FILE_TAG);
  ReaderSource<ModelReaderPtr> src(versionReader);
  ReadVersionT(src, version);
  return true;
}
}  // namespace version
