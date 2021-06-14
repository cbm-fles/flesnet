// Copyright 2013, 2015, 2021 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceAnalyzer.hpp"
#include "PatternChecker.hpp"
#include "TimesliceDebugger.hpp"
#include "Utility.hpp"
#include <boost/format.hpp>
#include <cassert>
#include <sstream>

TimesliceAnalyzer::TimesliceAnalyzer(uint64_t arg_output_interval,
                                     std::ostream& arg_out,
                                     std::string arg_output_prefix,
                                     std::ostream* arg_hist)
    : output_interval_(arg_output_interval), out_(arg_out),
      output_prefix_(std::move(arg_output_prefix)), hist_(arg_hist) {
  // create CRC-32C engine (Castagnoli polynomial)
  crc32_engine_ = crcutil_interface::CRC::Create(
      0x82f63b78, 0, 32, true, 0, 0, 0,
      crcutil_interface::CRC::IsSSE42Available(), NULL);
}

TimesliceAnalyzer::~TimesliceAnalyzer() {
  if (crc32_engine_ != nullptr) {
    crc32_engine_->Delete();
  }
}

uint32_t TimesliceAnalyzer::compute_crc(const fles::MicrosliceView& m) const {
  assert(crc32_engine_);

  crcutil_interface::UINT64 crc64 = 0;
  crc32_engine_->Compute(m.content(), m.desc().size, &crc64);

  return static_cast<uint32_t>(crc64);
}

bool TimesliceAnalyzer::check_crc(const fles::MicrosliceView& m) const {
  return compute_crc(m) == m.desc().crc;
}

bool TimesliceAnalyzer::check_microslice(const fles::MicrosliceView& m,
                                         size_t component,
                                         size_t microslice) {
// disabled, not applicable when using start time instead of index
#if 0
    if (m.desc().idx != microslice) {
        out_ << "microslice index " << m.desc().idx << " found in m.desc() "
             << microslice << std::endl;
        return false;
    }
#endif

  ++microslice_count_;
  content_bytes_ += m.desc().size;

  bool truncated =
      (m.desc().flags &
       static_cast<uint16_t>(fles::MicrosliceFlags::OverflowFlim)) != 0;
  if (truncated) {
    out_ << output_prefix_ << " microslice " << microslice
         << " truncated by FLIM" << std::endl;
  }

  bool pattern_error = !pattern_checkers_.at(component)->check(m);

  bool crc_error =
      ((m.desc().flags &
        static_cast<uint16_t>(fles::MicrosliceFlags::CrcValid)) != 0) &&
      !check_crc(m);
  if (crc_error) {
    out_ << output_prefix_ << "crc failure in microslice " << microslice
         << std::endl;
  }

  bool error = truncated || pattern_error || crc_error;

  // output ms stats
  if (hist_ != nullptr) {
    *hist_ << component << " " << microslice << " " << m.desc().eq_id << " "
           << m.desc().flags << " " << uint16_t(m.desc().sys_id) << " "
           << uint16_t(m.desc().sys_ver) << " " << m.desc().idx << " "
           << m.desc().size << " " << truncated << " " << pattern_error << " "
           << crc_error << "\n";
  }

  return !error;
}

void TimesliceAnalyzer::initialize(const fles::Timeslice& ts) {
  reference_descriptors_.clear();
  pattern_checkers_.clear();
  for (size_t c = 0; c < ts.num_components(); ++c) {
    assert(ts.num_microslices(c) > 0);
    fles::MicrosliceDescriptor desc = ts.get_microslice(c, 0).desc();
    reference_descriptors_.push_back(desc);
    pattern_checkers_.push_back(
        PatternChecker::create(desc.sys_id, desc.sys_ver, c));
  }
}

void TimesliceAnalyzer::print_reference() {
  out_ << output_prefix_ << "timeslice analyzer initialized with "
       << reference_descriptors_.size() << " components" << std::endl;
  for (size_t c = 0; c < reference_descriptors_.size(); ++c) {
    const fles::MicrosliceDescriptor& md = reference_descriptors_.at(c);
    out_ << output_prefix_ << "component " << c << ":"
         << boost::format(" eq_id=%04x") % static_cast<unsigned int>(md.eq_id)
         << boost::format(" sys=%02x-%02x") %
                static_cast<unsigned int>(md.sys_id) %
                static_cast<unsigned int>(md.sys_ver)
         << " "
         << fles::to_string(static_cast<fles::SubsystemIdentifier>(md.sys_id))
         << std::endl;
  }
}

bool TimesliceAnalyzer::check_timeslice(const fles::Timeslice& ts) {
  if (timeslice_count_ == 0) {
    initialize(ts);
    print_reference();
  }

  ++timeslice_count_;

  if (ts.num_components() == 0) {
    out_ << output_prefix_ << "no component in timeslice " << ts.index()
         << std::endl;
    ++timeslice_error_count_;
    return false;
  }

  uint64_t first_component_start_time = 0;
  if (ts.num_microslices(0) != 0) {
    first_component_start_time = ts.get_microslice(0, 0).desc().idx;
  }
  for (size_t c = 0; c < ts.num_components(); ++c) {
    if (ts.num_microslices(c) == 0) {
      out_ << output_prefix_ << "no microslices in timeslice " << ts.index()
           << ", component " << c << std::endl;
      ++timeslice_error_count_;
      return false;
    }
    // ensure all components start with same time
    uint64_t component_start_time = ts.get_microslice(c, 0).desc().idx;
    if (component_start_time != first_component_start_time) {
      out_ << output_prefix_ << "start time missmatch in timeslice "
           << ts.index() << ", component " << c << ", start time "
           << component_start_time << ", offset to c0 "
           << static_cast<int64_t>(first_component_start_time -
                                   component_start_time)
           << std::endl;
      ++timeslice_error_count_;
      return false;
    }
    // check all microslices of the component
    pattern_checkers_.at(c)->reset();
    for (size_t m = 0; m < ts.num_microslices(c); ++m) {
      bool success =
          check_microslice(ts.get_microslice(c, m), c,
                           ts.index() * ts.num_core_microslices() + m);
      if (!success) {
        out_ << output_prefix_ << "error in timeslice " << ts.index()
             << ", component " << c << ", microslice " << m << std::endl;
        if (timeslice_error_count_ == 0) { // full dump for first error
          out_ << output_prefix_ << "microslice descriptor:\n"
               << MicrosliceDescriptorDump(ts.get_microslice(c, m).desc())
               << std::flush;
          out_ << output_prefix_ << "microslice content:\n"
               << BufferDump(ts.get_microslice(c, m).content(),
                             ts.get_microslice(c, m).desc().size)
               << std::flush;
        }
        ++timeslice_error_count_;
        return false;
      }
    }
  }
  return true;
}

std::string TimesliceAnalyzer::statistics() const {
  std::stringstream s;
  s << "timeslices checked: " << timeslice_count_ << " ("
    << human_readable_count(content_bytes_, true) << " in " << microslice_count_
    << " microslices)";
  if (timeslice_error_count_ > 0) {
    s << " [errors: " << timeslice_error_count_ << "]";
  }
  return s.str();
}

void TimesliceAnalyzer::put(std::shared_ptr<const fles::Timeslice> timeslice) {
  check_timeslice(*timeslice);
  if ((timeslice_count_ % output_interval_) == 0) {
    out_ << output_prefix_ << statistics() << std::endl;
  }
}
