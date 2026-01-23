// Copyright 2025 Florian Schintke <schintke@zib.de>
/// \file
/// \brief Collection of helper functions regarding boost and boost archives.

#include "BoostHelper.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

namespace fles {
/**
 * \brief Find boost archive version of an archive ifstream.
 */
uint64_t boost_peek_for_archive_version(std::ifstream &stream) {
  // remember stream position
  auto pos = stream.tellg();

  // rewind the stream
  stream.seekg(0);
  // read similar to contrib/boost-magic
  // quad 22
  uint64_t beginning = 0;
  char eightBytes[9];
  stream.get(static_cast<char *>(eightBytes), 8+1); // 8 + '\0' auto appended
  for (auto i = 7; i >= 0; i--) {
    beginning = (beginning << 8) + eightBytes[i];
  }
  if (22 != beginning) {
    // not a boost archive file
    return 0;
  }
  // next: expect the string "serialization::archive"
  char str[23];
  stream.get(static_cast<char *>(str), 22+1); // 22 + '\0' auto appended
  std::string marker = std::string(static_cast<char *>(str),22);
  if (marker != std::string("serialization::archive")) {
    return 0;
  }
  // next: expect the archive version as 2-byte short
  char archivversbytes[3]; // 2 + '\0' auto appended
  stream.get(static_cast<char *>(archivversbytes), 2+1);
  uint16_t archivvers = (archivversbytes[1] << 8) + archivversbytes[0];

  // restore stream position
  stream.seekg(pos);
  return archivvers;
};

} // namespace fles
