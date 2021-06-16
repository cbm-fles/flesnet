// Copyright 2013, 2015, 2021 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceAnalyzer.hpp"
#include "PatternChecker.hpp"
#include "TimesliceDebugger.hpp"
#include "Utility.hpp"
#include <boost/format.hpp>
#include <cassert>
#include <optional>
#include <sstream>

// Aim for balance: the TimesliceAnalyzer should provide detailed information if
// an inconsistency is encountered in the data stream. On the other hand, it
// should never spam stdout so that it can be active even with continuing errors
// without affecting the run time significantly.

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
    out_ << output_prefix_ << "  component " << c << ":"
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

  // timeslice global checks
  if (ts.num_components() == 0) {
    if (output_active()) {
      out_ << output_prefix_ << "no component in timeslice " << ts.index()
           << std::endl;
    }
    return false;
  }

  bool ts_success = true;

  // check start time consistency of components
  bool start_time_mismatch = false;
  std::optional<uint64_t> reference_start_time;
  for (size_t c = 0; c < ts.num_components(); ++c) {
    if (ts.num_microslices(c) > 0) {
      const uint64_t component_start_time = ts.get_microslice(c, 0).desc().idx;
      if (reference_start_time.has_value()) {
        if (reference_start_time.value() != component_start_time) {
          start_time_mismatch = true;
        }
      } else {
        reference_start_time = component_start_time;
      }
    }
  }
  if (start_time_mismatch) {
    // dump start times for debugging
    if (output_active()) {
      out_ << output_prefix_ << "start time mismatch in timeslice "
           << ts.index() << std::endl;
      for (size_t c = 0; c < ts.num_components(); ++c) {
        out_ << output_prefix_ << "  component " << c
             << " start time: " << ts.get_microslice(c, 0).desc().idx
             << std::endl;
      }
    }
    ts_success = false;
  }

  // check the individual timeslice components
  for (size_t c = 0; c < ts.num_components(); ++c) {
    bool tsc_success = check_timeslice_component(ts, c);
    if (!tsc_success) {
      ++timeslice_component_error_count_;
      ts_success = false;
    }
  }

  return ts_success;
}

bool TimesliceAnalyzer::check_timeslice_component(const fles::Timeslice& ts,
                                                  size_t component) {
  ++timeslice_component_count_;
  bool tsc_success = true;

  if (ts.num_microslices(component) == 0) {
    if (output_active()) {
      out_ << output_prefix_ << "no microslices in timeslice " << ts.index()
           << ", component " << component << std::endl;
    }
    tsc_success = false;
  }

  // check all microslices of the component
  pattern_checkers_.at(component)->reset();
  for (size_t m = 0; m < ts.num_microslices(component); ++m) {
    bool ms_success =
        check_microslice(ts.get_microslice(component, m), component,
                         ts.index() * ts.num_core_microslices() + m);
    if (!ms_success) {
      ++microslice_error_count_;
      if (output_active()) {
        out_ << output_prefix_ << "error in timeslice " << ts.index()
             << ", component " << component << ", microslice " << m
             << std::endl;
        out_ << output_prefix_ << "microslice descriptor:\n"
             << MicrosliceDescriptorDump(ts.get_microslice(component, m).desc())
             << std::flush;
        out_ << output_prefix_ << "microslice content:\n"
             << BufferDump(ts.get_microslice(component, m).content(),
                           ts.get_microslice(component, m).desc().size)
             << std::flush;
      }
      tsc_success = false;
    }
  }

  // check start time consistency of microslices
  if (ts.num_microslices(component) >= 2) {
    uint64_t first = ts.get_microslice(component, 0).desc().idx;
    uint64_t second = ts.get_microslice(component, 1).desc().idx;
    if (second <= first) {
      if (output_active()) {
        out_ << output_prefix_ << "error in timeslice " << ts.index()
             << ", component " << component
             << ": start time not increasing in first two microslices"
             << std::endl;
        out_ << output_prefix_ << "microslice 0 descriptor:\n"
             << MicrosliceDescriptorDump(ts.get_microslice(component, 0).desc())
             << std::flush;
        out_ << output_prefix_ << "microslice 1 descriptor:\n"
             << MicrosliceDescriptorDump(ts.get_microslice(component, 1).desc())
             << std::flush;
      }
      tsc_success = false;
    } else {
      uint64_t reference_delta = second - first;
      for (size_t m = 2; m < ts.num_microslices(component); ++m) {
        uint64_t this_start_time = ts.get_microslice(component, m).desc().idx;
        uint64_t expected_start_time = first + m * reference_delta;
        if (this_start_time != expected_start_time) {
          if (output_active()) {
            out_ << output_prefix_ << "error in timeslice " << ts.index()
                 << ", component " << component
                 << ": unexpected start time in microslice " << m << std::endl;
            out_ << output_prefix_ << "microslice 0 descriptor:\n"
                 << MicrosliceDescriptorDump(
                        ts.get_microslice(component, 0).desc())
                 << std::flush;
            out_ << output_prefix_ << "microslice 1 descriptor:\n"
                 << MicrosliceDescriptorDump(
                        ts.get_microslice(component, 1).desc())
                 << std::flush;
            out_ << output_prefix_ << "microslice " << m << " descriptor:\n"
                 << MicrosliceDescriptorDump(
                        ts.get_microslice(component, m).desc())
                 << std::flush;
          }
          tsc_success = false;
        }
      }
    }
  }

  return tsc_success;
}

bool TimesliceAnalyzer::check_microslice(const fles::MicrosliceView& m,
                                         size_t component,
                                         size_t microslice) {
  ++microslice_count_;
  content_bytes_ += m.desc().size;

  bool truncated =
      (m.desc().flags &
       static_cast<uint16_t>(fles::MicrosliceFlags::OverflowFlim)) != 0;
  if (truncated && output_active()) {
    out_ << output_prefix_ << " microslice " << microslice
         << " truncated by FLIM" << std::endl;
  }

  bool pattern_error = !pattern_checkers_.at(component)->check(m);
  if (pattern_error && output_active()) {
    out_ << output_prefix_ << "pattern error in microslice " << microslice
         << std::endl;
  }

  bool crc_error =
      ((m.desc().flags &
        static_cast<uint16_t>(fles::MicrosliceFlags::CrcValid)) != 0) &&
      !check_crc(m);
  if (crc_error && output_active()) {
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

bool TimesliceAnalyzer::output_active() const {
  constexpr size_t limit = 20;
  return timeslice_error_count_ < limit &&
         timeslice_component_error_count_ < limit &&
         microslice_error_count_ < limit;
}

std::string TimesliceAnalyzer::statistics() const {
  std::stringstream s;
  s << "* checked " << timeslice_count_ << " ts, " << timeslice_component_count_
    << " tsc, " << microslice_count_ << " ms, "
    << human_readable_count(content_bytes_, true);
  if (timeslice_error_count_ > 0) {
    s << " [with errors: " << timeslice_error_count_ << "/"
      << timeslice_component_error_count_ << "/" << microslice_error_count_
      << "]";
  }
  return s.str();
}

void TimesliceAnalyzer::put(std::shared_ptr<const fles::Timeslice> timeslice) {
  bool success = check_timeslice(*timeslice);
  if (!success) {
    ++timeslice_error_count_;
  }
  if ((timeslice_count_ % output_interval_) == 0) {
    out_ << output_prefix_ << statistics() << std::endl;
  }
}
