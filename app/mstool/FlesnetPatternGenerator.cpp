// Copyright 2012-2014 Jan de Cuveland <cmail@cuveland.de>

#include "FlesnetPatternGenerator.hpp"
#include "MicrosliceDescriptor.hpp"
#include "StorableMicroslice.hpp"
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

FlesnetPatternGenerator::FlesnetPatternGenerator(uint64_t input_index,
                                                 uint32_t typical_content_size,
                                                 bool randomize_sizes)
    : input_index_(input_index), typical_content_size_(typical_content_size),
      randomize_sizes_(randomize_sizes),
      random_distribution_(typical_content_size) {}

fles::Microslice* FlesnetPatternGenerator::do_get() {
  unsigned int content_bytes = typical_content_size_;
  if (randomize_sizes_) {
    content_bytes = random_distribution_(random_generator_);
  }
  content_bytes &= ~0x7u; // round down to multiple of sizeof(uint64_t)

  std::vector<uint8_t> content(content_bytes);
  uint32_t crc = 0x00000000;
  for (uint64_t i = 0; i < content_bytes; i += sizeof(uint64_t)) {
    uint64_t data_word = (input_index_ << 48L) | i;
    std::memcpy(content.data() + i, &data_word, sizeof(data_word));
    crc ^= (data_word & 0xffffffff) ^ (data_word >> 32L);
  }

  const auto hdr_id =
      static_cast<uint8_t>(fles::HeaderFormatIdentifier::Standard);
  const auto hdr_ver =
      static_cast<uint8_t>(fles::HeaderFormatVersion::Standard);
  const uint16_t eq_id = 0xE001;
  const uint16_t flags = 0x0000;
  const auto sys_id = static_cast<uint8_t>(fles::Subsystem::FLES);
  const auto sys_ver =
      static_cast<uint8_t>(fles::SubsystemFormatFLES::BasicRampPattern);
  const fles::MicrosliceDescriptor desc{
      hdr_id,  hdr_ver,           eq_id, flags,         sys_id,
      sys_ver, microslice_count_, crc,   content_bytes, content_bytes_count_};

  ++microslice_count_;
  content_bytes_count_ += content_bytes;

  return new fles::StorableMicroslice(desc, std::move(content)); // NOLINT
}
