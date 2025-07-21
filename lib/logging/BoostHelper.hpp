// Copyright 2025 Florian Schintke <schintke@zib.de>
/// \file
/// \brief Collection of helper functions regarding boost and boost archives.
#pragma once

#include <cstdint>

namespace fles {
/**
 * \brief Find earliest boost library supporting a given boost archive version.
 */
constexpr const char* boostlib_for_archive_version(uint64_t v) {
  switch (v) {
    // earliest boost library version known to support the given
    // archive version according to doc/boost_serialization.txt and
    // https://github.com/boostorg/serialization/blob/develop/src/basic_archive.cpp
  case 10: return "1.54";
  case 11: return "1.56";
  case 12: return "1.58";
  case 13: return "1.59";
  case 14: return "1.60";
  case 15: return "1.64";
  case 16: return "1.66";
  case 17: return "1.68";
  case 18: return "1.73";
  case 19: return "1.76";
  case 20: return "1.84";
  default:
    // latest known version of boost that still uses the largest
    // explicitly listed archive version above
    return "newer than 1.88";
  }
}

} // namespace fles
