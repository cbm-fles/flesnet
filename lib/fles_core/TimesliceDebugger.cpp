// Copyright 2013-2015 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceDebugger.hpp"
#include <algorithm>
#include <boost/format.hpp>
#include <cstdint>
#include <iostream>

std::ostream& TimesliceDump::write_to_stream(std::ostream& s) const {
  uint64_t min_num_microslices = UINT64_MAX;
  uint64_t max_num_microslices = 0;
  uint64_t total_num_microslices = 0;
  uint64_t min_microslice_size = UINT64_MAX;
  uint64_t max_microslice_size = 0;
  uint64_t total_microslice_size = 0;

  for (uint64_t c = 0; c < ts.num_components(); ++c) {
    uint64_t num_microslices = ts.num_microslices(c);
    min_num_microslices = std::min(min_num_microslices, num_microslices);
    max_num_microslices = std::max(max_num_microslices, num_microslices);
    total_num_microslices += num_microslices;
    for (uint64_t m = 0; m < num_microslices; ++m) {
      uint64_t size = ts.descriptor(c, m).size;
      total_microslice_size += size;
      min_microslice_size = std::min(min_microslice_size, size);
      max_microslice_size = std::max(max_microslice_size, size);
    }
  }

  uint64_t min_overlap = min_num_microslices - ts.num_core_microslices();
  uint64_t max_overlap = max_num_microslices - ts.num_core_microslices();

  s << "timeslice " << ts.index() << " size: " << ts.num_components() << " x "
    << ts.num_core_microslices() << " microslices";
  if (ts.num_components() != 0) {
    s << " (+";
    if (min_overlap != max_overlap) {
      s << min_overlap << ".." << max_overlap;
    } else {
      s << min_overlap;
    }
    s << " overlap) = " << total_num_microslices << "\n";
    s << "\tmicroslice size min/avg/max: " << min_microslice_size << "/"
      << (static_cast<double>(total_microslice_size) / total_num_microslices)
      << "/" << max_microslice_size << "\n";
  }

  if (verbosity > 1) {
    for (uint64_t c = 0; c < ts.num_components(); ++c) {
      uint64_t num_microslices = ts.num_microslices(c);
      for (uint64_t m = 0; m < num_microslices; ++m) {
        s << "timeslice " << ts.index() << " microslice " << m << " component "
          << c << "\n"
          << ts.get_microslice(c, m) << std::endl;
      }
    }
  }

  return s;
}

std::ostream& MicrosliceDescriptorDump::write_to_stream(std::ostream& s) const {
  s << "hi hv eqid flag si sv idx/start        crc      size     offset\n";
  s << boost::format(
           "%02x %02x %04x %04x %02x %02x %016lx %08x %08x %016lx\n") %
           static_cast<unsigned int>(md.hdr_id) %
           static_cast<unsigned int>(md.hdr_ver) % md.eq_id % md.flags %
           static_cast<unsigned int>(md.sys_id) %
           static_cast<unsigned int>(md.sys_ver) % md.idx % md.crc % md.size %
           md.offset;

  return s;
}

std::ostream& BufferDump::write_to_stream(std::ostream& s) const {
  constexpr int bytes_per_block = 8;
  constexpr int bytes_per_line = 4 * bytes_per_block;

  for (int i = 0; i < static_cast<int>(size); i += bytes_per_line) {
    for (int j = bytes_per_line - 1; j >= 0; --j) {
      if (i + j >= static_cast<int>(size)) {
        s << "  ";
      } else {
        s << boost::format("%02x") %
                 static_cast<int>(static_cast<const uint8_t*>(buf)[i + j]);
      }
      if ((j % bytes_per_block) == 0) {
        s << "  ";
      }
    }
    s << boost::format(":%04x\n") % i;
  }

  return s;
}
