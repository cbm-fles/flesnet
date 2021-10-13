// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "MicrosliceAnalyzer.hpp"
#include "PatternChecker.hpp"
#include "TimesliceDebugger.hpp"
#include "Utility.hpp"
#include <cassert>
#include <sstream>

MicrosliceAnalyzer::MicrosliceAnalyzer(uint64_t arg_output_interval,
                                       size_t arg_out_verbosity,
                                       std::ostream& arg_out,
                                       std::string arg_output_prefix,
                                       size_t component)
    : output_interval_(arg_output_interval), out_verbosity_(arg_out_verbosity),
      out_(arg_out), output_prefix_(std::move(arg_output_prefix)),
      component_(component) {
  // create CRC-32C engine (Castagnoli polynomial)
  crc32_engine_ = crcutil_interface::CRC::Create(
      0x82f63b78, 0, 32, true, 0, 0, 0,
      crcutil_interface::CRC::IsSSE42Available(), nullptr);
}

MicrosliceAnalyzer::~MicrosliceAnalyzer() {
  if (crc32_engine_ != nullptr) {
    crc32_engine_->Delete();
  }
}

uint32_t MicrosliceAnalyzer::compute_crc(const fles::Microslice& ms) const {
  assert(crc32_engine_);

  crcutil_interface::UINT64 crc64 = 0;
  crc32_engine_->Compute(ms.content(), ms.desc().size, &crc64);

  return static_cast<uint32_t>(crc64);
}

bool MicrosliceAnalyzer::check_crc(const fles::Microslice& ms) const {
  return compute_crc(ms) == ms.desc().crc;
}

void MicrosliceAnalyzer::initialize(const fles::Microslice& ms) {
  fles::MicrosliceDescriptor desc = ms.desc();
  reference_descriptor_ = desc;
  pattern_checker_ =
      PatternChecker::create(desc.sys_id, desc.sys_ver, component_);
}

bool MicrosliceAnalyzer::check_microslice(const fles::Microslice& ms) {
  bool result = true;

  if (microslice_count_ == 0) {
    initialize(ms);
    if (out_verbosity_ >= 2) {
      out_ << output_prefix_ << "eq_id=" << std::hex << std::showbase
           << ms.desc().eq_id << std::endl;
      out_ << output_prefix_
           << "sys_id=" << static_cast<uint32_t>(ms.desc().sys_id) << std::endl;
      out_ << output_prefix_
           << "sys_ver=" << static_cast<uint32_t>(ms.desc().sys_ver) << std::dec
           << std::endl;
      out_ << output_prefix_ << "start=" << ms.desc().idx << "ns" << std::endl;
    }
  } else if (microslice_count_ == 1) {
    reference_delta_t_ = ms.desc().idx - previous_start_;
    if (out_verbosity_ >= 2) {
      out_ << output_prefix_ << "delta_t=" << reference_delta_t_ << "ns"
           << std::endl;
    }
  } else {
    uint64_t delta_t = ms.desc().idx - previous_start_;
    if (delta_t != reference_delta_t_) {
      if (out_verbosity_ >= 3) {
        out_ << output_prefix_ << "delta_t=" << delta_t << "ns"
             << " in microslice " << microslice_count_ << std::endl;
      }
      result = false;
    }
  }

  if ((ms.desc().flags &
       static_cast<uint16_t>(fles::MicrosliceFlags::OverflowFlim)) != 0) {
    if (out_verbosity_ >= 3) {
      out_ << output_prefix_ << "data truncated by FLIM in microslice "
           << microslice_count_ << std::endl;
    }
    ++microslice_truncated_count_;
  }

  if (!pattern_checker_->check(ms)) {
    if (out_verbosity_ >= 3) {
      out_ << output_prefix_ << "pattern error in microslice "
           << microslice_count_ << std::endl;
    }
    result = false;
  }

  if (((ms.desc().flags &
        static_cast<uint16_t>(fles::MicrosliceFlags::CrcValid)) != 0) &&
      !check_crc(ms)) {
    if (out_verbosity_ >= 3) {
      out_ << output_prefix_ << "crc failure in microslice "
           << microslice_count_ << std::endl;
    }
    result = false;
  }

  if (!result) {
    if (microslice_error_count_ < 3 &&
        out_verbosity_ >= 3) { // full dump for first 3 error
      out_ << "microslice content:\n"
           << MicrosliceDescriptorDump(ms.desc())
           << BufferDump(ms.content(), ms.desc().size) << std::flush;
    }
    ++microslice_error_count_;
  }

  ++microslice_count_;
  content_bytes_ += ms.desc().size;
  previous_start_ = ms.desc().idx;

  return result;
}

std::string MicrosliceAnalyzer::statistics() const {
  std::stringstream s;
  s << "microslices checked: " << microslice_count_ << " ("
    << human_readable_count(content_bytes_, true);
  s.precision(1);
  s << ", BER < " << std::scientific
    << calculate_ber(content_bytes_, microslice_error_count_) << ")";
  if (microslice_error_count_ > 0) {
    s << " [" << microslice_error_count_ << " errors]";
  }
  return s.str();
}

void MicrosliceAnalyzer::put(std::shared_ptr<const fles::Microslice> ms) {
  if (!check_microslice(*ms)) {
    // for now we do not reset the checker to follow ms count across errors
    // pattern_checker_->reset();
  }
  if ((microslice_count_ % output_interval_) == 0) {
    out_ << output_prefix_ << statistics() << std::endl;
  }
}
