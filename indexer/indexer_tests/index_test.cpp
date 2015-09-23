#include "../../testing/testing.hpp"
#include "../index.hpp"

#include "../../base/macros.hpp"
#include "../../base/stl_add.hpp"

#include "../../defines.hpp"

#include "../../std/string.hpp"


UNIT_TEST(IndexParseTest)
{
  Index index;
  index.Add("minsk-pass" DATA_FILE_EXTENSION);

  // Make sure that index is actually parsed.
  NoopFunctor fn;
  index.ForEachInScale(fn, 15);
}
