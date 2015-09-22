#pragma once
#include "common_defines.hpp"

#ifdef new
#undef new
#endif

#include <algorithm>

using std::equal;
using std::find;
using std::lexicographical_compare;
using std::lower_bound;
using std::max;
using std::max_element;
using std::min;
using std::next_permutation;
using std::sort;
using std::stable_sort;
using std::swap;
using std::upper_bound;
using std::unique;
using std::equal_range;
using std::for_each;
using std::copy;
using std::remove_if;
using std::replace;
using std::reverse;
using std::set_union;
using std::set_intersection;
using std::set_difference;
using std::set_symmetric_difference;
using std::swap;
using std::transform;

#ifdef DEBUG_NEW
#define new DEBUG_NEW
#endif
