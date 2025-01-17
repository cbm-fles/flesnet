// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2025 Dirk Hutter <cmail@dirk-hutter.de>

// Info for FLIM pattern checking:
// Timeslices are treated as independet
// Microslices are treated as consecutive data stream
// Checks for:
// - cosecutive pgen sequence numbers across all microslices
//   in one timeslice component
// Implementation is not dump parallelizable across ts components!

#include "FlimPatternChecker.hpp"
#include <iostream>

bool FlimPatternChecker::check(const fles::Microslice& m) {
  uint8_t last_word_size = 0;

  // increment packte number if initialized
  if (pgen_packet_number_ != 0) {
    ++pgen_packet_number_;
  }

  if (m.desc().size >= 1) {
    last_word_size = m.content()[0];
    // Assert last word size is in valid range form 0 to 32 bytes
    if (last_word_size > 32) {
      std::cerr << "Flim pgen: error in last word size: out of bounds"
                << std::endl;
      std::cerr << "last word " << static_cast<uint32_t>(last_word_size)
                << std::endl;
      last_word_size = 0;
      return false;
    }
    // Do not check last word size consistency and last word content if ms was
    // truncated
    if ((m.desc().flags &
         static_cast<uint16_t>(fles::MicrosliceFlags::OverflowFlim)) == 0) {
      if ((m.desc().size & 0x1F) != (last_word_size & 0x1F)) {
        std::cerr
            << "Flim pgen: error in last word size: inconsistent with desc.size"
            << std::endl;
        std::cerr << "desc.size " << m.desc().size << std::endl;
        std::cerr << "last word " << static_cast<uint32_t>(last_word_size)
                  << std::endl;
        return false;
      }
    } else {
      // if truncated set to 0 to skip last word content check at the end
      last_word_size = 0;
    }
  }

  if (m.desc().size >= 4) {
    const uint16_t word = reinterpret_cast<const uint16_t*>(m.content())[1];
    if (word != 0xBBFF) {
      std::cerr << "Flim pgen: error in hdr word. Found " << std::hex << word
                << std::endl;
      return false;
    }
  }

  if (m.desc().size >= 8) {
    uint32_t pgen_packet_number =
        reinterpret_cast<const uint32_t*>(m.content())[1];
    // check if initalized
    if (pgen_packet_number_ != 0 && pgen_packet_number_ != pgen_packet_number) {
      std::cerr << "Flim pgen: error in packet number;"
                << " exp " << std::hex << pgen_packet_number_ << " seen "
                << pgen_packet_number << std::endl;
      return false;
    }
    // initialize if uninitialized
    if (pgen_packet_number_ == 0) {
      pgen_packet_number_ = pgen_packet_number;
    }
  }

  if (m.desc().size >= 16) {
    uint64_t pgen_timestamp = reinterpret_cast<const uint64_t*>(m.content())[1];
    if (m.desc().idx != pgen_timestamp) {
      std::cerr << "Flim pgen: error in packet timestamp; "
                << " exp " << std::hex << m.desc().idx << " seen "
                << pgen_timestamp << std::endl;
      return false;
    }
  }

  // check ramp, everything from third 64-bit word but last 256-bit word
  const uint64_t* content = reinterpret_cast<const uint64_t*>(m.content());

  // number of full 256-bit words
  size_t size256 = m.desc().size / 32;
  if (last_word_size == 32 && size256 > 0) {
    // special case: last full 256-bit word is not from ramp generator
    size256--;
  }
  size_t size64 = size256 * 4;
  if (m.desc().size < 32) {
    // special case: header word is truncated last word
    size64 = m.desc().size / 8;
  }

  for (size_t pos = 2; pos < size64; ++pos) {
    uint64_t expect = 0xABCD000000000000 + pos - 2;
    if (content[pos] != expect) {
      std::cerr << "Flim pgen: error in ramp word " << (pos - 2) << " exp "
                << std::hex << expect << " seen " << content[pos] << std::endl;
      return false;
    }
  }

  // check last word if any
  if (m.desc().size > 32) {
    size_t last_word_start = size256 * 32;
    for (size_t pos = 0; pos < last_word_size; ++pos) {
      uint8_t data = m.content()[last_word_start + pos];
      uint8_t expect = 0xA0 + pos;
      if (data != expect) {
        std::cerr << "Flim pgen: error in last word at pos " << pos << " exp "
                  << std::hex << expect << " seen " << data << std::endl;
        return false;
      }
    }
  }

  return true;
}
