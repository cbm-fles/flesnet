// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>

// Info for FLIB pattern checking:
// Timeslices are treated as independet
// Microslices are treated as consecutive data stream
// Checks for:
// - consecutive CBMnet frame nubers across all microclices
//   in one timeslice component
// - cosecutive pgen sequence numbers across all microslices
//   in one timeslice component
// Implementation is not dump parallelizable across ts components!

#include "FlibLegacyPatternChecker.hpp"

#include "Microslice.hpp"
#include "MicrosliceDescriptor.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

bool FlibLegacyPatternChecker::check_content_pgen(const uint16_t* content,
                                                  size_t size) {
  constexpr uint16_t source_address = 0;

  if (content[0] != source_address) {
    std::cerr << "unexpected source address: " << content[0] << std::endl;
    return false;
  }

  for (size_t i = 1; i < size - 1; ++i) {
    uint8_t low = content[i] & 0xff;
    uint8_t high = (content[i] >> 8) & 0xff;
    if (high != 0xbc || low != i - 1) {
      std::cerr << "unexpected cbmnet content word: " << content[i]
                << std::endl;
      return false;
    }
  }

  uint16_t pgen_sequence_number = content[size - 1];
  uint16_t expected_pgen_sequence_number = pgen_sequence_number_ + 1;
  // uncomment to check only for increasing sequence numbers
  //    if (pgen_sequence_number_ != 0 && pgen_sequence_number != 0 &&
  //        pgen_sequence_number < expected_pgen_sequence_number) {
  if (pgen_sequence_number_ != 0 &&
      pgen_sequence_number != expected_pgen_sequence_number) {
    std::cerr << "unexpected pgen sequence number in frame "
              << static_cast<unsigned>(frame_number_) << ":  expected "
              << expected_pgen_sequence_number << "  found "
              << pgen_sequence_number << std::endl;
    return false;
  }
  pgen_sequence_number_ = pgen_sequence_number;

  return true;
}

bool FlibLegacyPatternChecker::check_cbmnet_frames(const uint16_t* content,
                                                   size_t size,
                                                   uint8_t sys_id,
                                                   uint8_t sys_ver) {
  size_t i = 0;
  while (i < size) {
    uint8_t frame_number = (content[i] >> 8) & 0xff;
    uint8_t word_count =
        (content[i] & 0xff) + 1; // FIXME: Definition in hardware
    uint8_t padding_count = (4 - ((word_count + 1) & 0x3)) & 0x3;
    ++i;

    uint8_t expected_frame_number = frame_number_ + 1;
    if (frame_number_ != 0 && frame_number != expected_frame_number) {
      std::cerr << "unexpected cbmnet frame number:"
                << "  expected: "
                << static_cast<uint32_t>(expected_frame_number)
                << "  found: " << static_cast<uint32_t>(frame_number)
                << std::endl;
      return false;
    }
    frame_number_ = frame_number;

    if (word_count < 4 || word_count > 64 ||
        i + word_count + padding_count > size) {
      std::cerr << "invalid cbmnet frame word count: " << word_count
                << std::endl;
      return false;
    }

    if (sys_id == static_cast<uint8_t>(fles::Subsystem::FLES) &&
        sys_ver ==
            static_cast<uint8_t>(fles::SubsystemFormatFLES::CbmNetPattern)) {
      if (!check_content_pgen(&content[i], word_count)) {
        return false;
      }
    }
    i += word_count + padding_count;
  }

  return true;
}

bool FlibLegacyPatternChecker::check(const fles::Microslice& m) {
  const auto* content = reinterpret_cast<const uint64_t*>(m.content());
  if (content[0] != reinterpret_cast<const uint64_t*>(&m.desc())[0] ||
      content[1] != reinterpret_cast<const uint64_t*>(&m.desc())[1]) {
    return false;
  }
  return check_cbmnet_frames(reinterpret_cast<const uint16_t*>(&content[2]),
                             (m.desc().size - 16) / sizeof(uint16_t),
                             m.desc().sys_id, m.desc().sys_ver);
}
