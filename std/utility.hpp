#pragma once
#include "common_defines.hpp"

#ifdef new
#undef new
#endif

#include <utility>
using std::pair;
using std::make_pair;

#ifdef DEBUG_NEW
#define new DEBUG_NEW
#endif
