// Copyright 2013, 2015, 2021, 2023 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceAnalyzer.hpp"
#include "MicrosliceDescriptor.hpp"
#include "Monitor.hpp"
#include "PatternChecker.hpp"
#include "System.hpp"
#include "Timeslice.hpp"
#include "TimesliceDebugger.hpp"
#include "Utility.hpp"
#include "interface.h"
#include "log_short.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Aim for balance: the TimesliceAnalyzer should provide detailed information if
// an inconsistency is encountered in the data stream. On the other hand, it
// should never spam stdout so that it can be active even with continuing errors
// without affecting the run time significantly.

namespace {
std::string ns_to_string(uint64_t time_ns) {
  std::stringstream ss;
  ss.imbue(std::locale("")); // use locale for thousands separator
  ss << time_ns << " ns";
  return ss.str();
}

std::string ns_timestamp(uint64_t time_ns) {
  // Chrono time_point from nanoseconds
  std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>
      tp{std::chrono::nanoseconds{time_ns}};
  // Convert to time_t and remaining nanoseconds
  auto tp_seconds = std::chrono::time_point_cast<std::chrono::seconds>(tp);
  std::time_t t = std::chrono::system_clock::to_time_t(tp_seconds);
  auto rem_ns = (tp.time_since_epoch() - std::chrono::seconds{t}).count();

  std::tm tm = *std::localtime(&t);
  std::stringstream ss;
  ss.imbue(std::locale("")); // use locale for thousands separator
  ss << std::put_time(&tm, "%F %T") << " " << std::setfill('0') << std::setw(9)
     << rem_ns << " " << std::put_time(&tm, "%Z");
  return ss.str();
}

// The output of a dump, each line indented, without a trailing new line
template <typename Dump> std::string indented(const Dump& dump) {
  std::stringstream ss;
  ss << dump;
  std::string text;
  for (std::string line; std::getline(ss, line);) {
    if (!text.empty()) {
      text += '\n';
    }
    text += "  " + line;
  }
  return text;
}

// "c2/m3", "c2" or "", for messages about a timeslice
std::string location_string(std::optional<size_t> c = std::nullopt,
                            std::optional<size_t> m = std::nullopt) {
  std::string location;
  if (c) {
    location = "c" + std::to_string(*c);
  }
  if (m) {
    location += "/m" + std::to_string(*m);
  }
  return location;
}
} // namespace

TimesliceAnalyzer::TimesliceAnalyzer(uint64_t arg_output_interval,
                                     std::string arg_output_prefix,
                                     std::ostream* arg_hist,
                                     cbm::Monitor* monitor)
    : output_interval_(arg_output_interval),
      output_prefix_(std::move(arg_output_prefix)), hist_(arg_hist),
      previous_output_time_(std::chrono::system_clock::now()),
      monitor_(monitor) {
  // create CRC-32C engine (Castagnoli polynomial)
  crc32_engine_ = crcutil_interface::CRC::Create(
      0x82f63b78, 0, 32, true, 0, 0, 0,
      crcutil_interface::CRC::IsSSE42Available(), nullptr);

  hostname_ = fles::system::current_hostname();

  report_status();
}

TimesliceAnalyzer::~TimesliceAnalyzer() {
  if (crc32_engine_ != nullptr) {
    crc32_engine_->Delete();
  }
}

void TimesliceAnalyzer::put(std::shared_ptr<const fles::Timeslice> timeslice) {
  bool success = check_timeslice(*timeslice);
  if (!success) {
    ++timeslice_error_count_;
  }
  scheduler_.timer();

  // print statistics if either the next round number (multiple of
  // output_interval_) of timeslices is reached or a given time has passed
  auto now = std::chrono::system_clock::now();
  if ((timeslice_count_ % output_interval_) == 0 ||
      previous_output_time_ + output_time_interval_ <= now) {
    log_statistics();
    previous_output_time_ = now;
  }
}

void TimesliceAnalyzer::initialize(const fles::Timeslice& ts) {
  start_index_ = ts.index();
  reference_descriptors_.clear();
  pattern_checkers_.clear();
  for (size_t c = 0; c < ts.num_components(); ++c) {
    assert(ts.num_microslices(c) > 0);
    fles::MicrosliceDescriptor desc = ts.get_microslice(c, 0).desc();
    reference_descriptors_.push_back(desc);
    pattern_checkers_.push_back(
        PatternChecker::create(desc.sys_id, desc.sys_ver));
  }
}

bool TimesliceAnalyzer::check_timeslice(const fles::Timeslice& ts) {
  if (timeslice_count_ == 0) {
    initialize(ts);
    log_reference();
    // log timeslice duration
    if (ts.num_core_microslices() > 1 && ts.num_components() > 0) {
      uint64_t ms_length = ts.descriptor(0, 1).idx - ts.descriptor(0, 0).idx;
      uint64_t ts_length = ts.num_core_microslices() * ms_length;
      STATUS("ts duration: {} ({} * {})", ns_to_string(ts_length),
             logging::field("core_microslices", ts.num_core_microslices()),
             ns_to_string(ms_length),
             logging::field("ts_duration_ns", ts_length),
             logging::field("ms_duration_ns", ms_length));
    }
    // log start time
    STATUS("start time: {}", ns_timestamp(ts.start_time()),
           logging::field("start_time_ns", ts.start_time()));
  } else if (timeslice_count_ == 1) {
    // log timeslice start index and stride
    uint64_t stride = ts.index() - start_index_;
    STATUS("start index: {} (stride: {})",
           logging::field("start_index", start_index_),
           logging::field("stride", stride));
  }

  ++timeslice_count_;
  timeslice_index_ = ts.index();

  // timeslice global checks
  if (ts.num_components() == 0) {
    if (output_active()) {
      TS_ERROR(ts.index(), "error: no component in timeslice");
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
      if (reference_start_time) {
        if (*reference_start_time != component_start_time) {
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
      TS_ERROR(ts.index(), "error: start time mismatch");
      for (size_t c = 0; c < ts.num_components(); ++c) {
        TS_STATUS(ts.index(), "  component {} start time: {}",
                  logging::field("component", c),
                  logging::field("start_time_ns",
                                 ts.get_microslice(c, 0).desc().idx));
      }
    }
    ts_success = false;
  }

  // check the individual timeslice components
  for (size_t c = 0; c < ts.num_components(); ++c) {
    bool component_success = check_component(ts, c);
    if (!component_success) {
      ++component_error_count_;
      ts_success = false;
    }
  }

  return ts_success;
}

bool TimesliceAnalyzer::check_component(const fles::Timeslice& ts, size_t c) {
  ++component_count_;
  bool component_success = true;

  if (ts.num_microslices(c) == 0) {
    if (output_active()) {
      TS_ERROR(ts.index(), "error in {}: no microslices in component",
               location_string(c), logging::field("component", c));
    }
    component_success = false;
  }

  // check the individual microslices of the component
  pattern_checkers_.at(c)->reset();
  for (size_t m = 0; m < ts.num_microslices(c); ++m) {
    bool microslice_success = check_microslice(ts, c, m);
    if (!microslice_success) {
      ++microslice_error_count_;
      component_success = false;
    }
  }

  // check start time consistency of microslices
  if (ts.num_microslices(c) >= 2) {
    uint64_t first = ts.get_microslice(c, 0).desc().idx;
    uint64_t second = ts.get_microslice(c, 1).desc().idx;
    if (second <= first) {
      if (output_active()) {
        TS_ERROR(ts.index(),
                 "error in {}: start time not increasing in first two "
                 "microslices",
                 location_string(c), logging::field("component", c));
        log_microslice_descriptor(ts, c, 0);
        log_microslice_descriptor(ts, c, 1);
      }
      component_success = false;
    } else {
      uint64_t reference_delta = second - first;
      for (size_t m = 2; m < ts.num_microslices(c); ++m) {
        uint64_t this_start_time = ts.get_microslice(c, m).desc().idx;
        uint64_t expected_start_time = first + m * reference_delta;
        if (this_start_time != expected_start_time) {
          if (output_active()) {
            TS_ERROR(ts.index(),
                     "error in {}: unexpected microslice start time",
                     location_string(c, m), logging::field("component", c),
                     logging::field("microslice", m));
            log_microslice_descriptor(ts, c, 0);
            log_microslice_descriptor(ts, c, 1);
            log_microslice_descriptor(ts, c, m);
          }
          component_success = false;
        }
      }
    }
  }

  return component_success;
}

bool TimesliceAnalyzer::check_microslice(const fles::Timeslice& ts,
                                         size_t c,
                                         size_t m) {
  auto mv = ts.get_microslice(c, m);
  const auto& d = mv.desc();

  ++microslice_count_;
  content_bytes_ += d.size;
  bool error = false;

  // static descriptor checks
  if (d.hdr_id != 0xdd || d.hdr_ver != 0x01) {
    error = true;
    if (output_active()) {
      TS_ERROR(ts.index(),
               "error in {}: unknown header format in microslice descriptor",
               location_string(c, m), logging::field("component", c),
               logging::field("microslice", m));
      log_microslice_descriptor(ts, c, m);
    }
  }

  // check descriptor consistency
  auto r = reference_descriptors_.at(c);
  if (d.eq_id != r.eq_id || d.sys_id != r.sys_id || d.sys_ver != r.sys_ver) {
    error = true;
    if (output_active()) {
      TS_ERROR(ts.index(),
               "error in {}: unexpected change in microslice descriptor",
               location_string(c, m), logging::field("component", c),
               logging::field("microslice", m));
      log_microslice_descriptor(ts, c, m);
    }
  }

  bool truncated =
      (d.flags & static_cast<uint16_t>(fles::MicrosliceFlags::OverflowFlim)) !=
      0;
  if (truncated && output_active()) {
    TS_ERROR(ts.index(), "error in {}: microslice truncated by FLIM",
             location_string(c, m), logging::field("component", c),
             logging::field("microslice", m));
    log_microslice_descriptor(ts, c, m);
    log_microslice_content(ts, c, m);
  }

  bool pattern_error = !pattern_checkers_.at(c)->check(mv);
  if (pattern_error && output_active()) {
    TS_ERROR(ts.index(), "error in {}: pattern error", location_string(c, m),
             logging::field("component", c), logging::field("microslice", m));
    log_microslice_descriptor(ts, c, m);
    log_microslice_content(ts, c, m);
  }

  bool crc_error =
      ((d.flags & static_cast<uint16_t>(fles::MicrosliceFlags::CrcValid)) !=
       0) &&
      !check_crc(mv);
  if (crc_error && output_active()) {
    TS_ERROR(ts.index(), "error in {}: crc failure", location_string(c, m),
             logging::field("component", c), logging::field("microslice", m));
    log_microslice_descriptor(ts, c, m);
    log_microslice_content(ts, c, m);
  }

  error |= truncated || pattern_error || crc_error;

  // output ms stats
  if (hist_ != nullptr) {
    *hist_ << c << " " << m << " " << d.eq_id << " " << d.flags << " "
           << uint16_t(d.sys_id) << " " << uint16_t(d.sys_ver) << " " << d.idx
           << " " << d.size << " " << truncated << " " << pattern_error << " "
           << crc_error << "\n";
  }

  return !error;
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

void TimesliceAnalyzer::log_reference() {
  STATUS("timeslice analyzer initialized with {} components",
         logging::field("components", reference_descriptors_.size()));
  for (size_t c = 0; c < reference_descriptors_.size(); ++c) {
    const fles::MicrosliceDescriptor& md = reference_descriptors_.at(c);
    STATUS("  component {}: eq_id={:04x} sys={:02x}-{:02x} {}",
           logging::field("component", c), md.eq_id, md.sys_id, md.sys_ver,
           fles::to_string(static_cast<fles::Subsystem>(md.sys_id)),
           logging::field("eq_id", md.eq_id),
           logging::field("sys_id", md.sys_id),
           logging::field("sys_ver", md.sys_ver));
  }
}

void TimesliceAnalyzer::log_microslice_descriptor(const fles::Timeslice& ts,
                                                  size_t c,
                                                  size_t m) {
  TS_STATUS(ts.index(), "microslice descriptor of {}:\n{}",
            location_string(c, m),
            indented(MicrosliceDescriptorDump(ts.get_microslice(c, m).desc())),
            logging::field("component", c), logging::field("microslice", m));
}

void TimesliceAnalyzer::log_microslice_content(const fles::Timeslice& ts,
                                               size_t c,
                                               size_t m) {
  TS_STATUS(ts.index(), "microslice content of {}:\n{}", location_string(c, m),
            indented(BufferDump(ts.get_microslice(c, m).content(),
                                ts.get_microslice(c, m).desc().size)),
            logging::field("component", c), logging::field("microslice", m));
}

void TimesliceAnalyzer::log_statistics() const {
  std::string errors;
  if (timeslice_error_count_ > 0) {
    errors = " [with errors: " + std::to_string(timeslice_error_count_) +
             "ts/" + std::to_string(component_error_count_) + "c/" +
             std::to_string(microslice_error_count_) + "m]";
  }
  STATUS("* checked {} ts, {} c, {} m, {}{}",
         logging::field("timeslices", timeslice_count_),
         logging::field("components", component_count_),
         logging::field("microslices", microslice_count_),
         human_readable_count(content_bytes_, true), errors,
         logging::field("content_bytes", content_bytes_),
         logging::field("timeslice_errors", timeslice_error_count_),
         logging::field("component_errors", component_error_count_),
         logging::field("microslice_errors", microslice_error_count_));
}

bool TimesliceAnalyzer::output_active() const {
  constexpr size_t limit = 10;
  return timeslice_error_count_ < limit && component_error_count_ < limit &&
         microslice_error_count_ < limit;
}

void TimesliceAnalyzer::report_status() {
  constexpr auto interval = std::chrono::seconds(1);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  if (monitor_ != nullptr) {
    const std::string prefix = output_prefix_.empty() ? ":" : output_prefix_;
    monitor_->QueueMetric("timeslice_analyzer_status",
                          {{"host", hostname_}, {"output_prefix", prefix}},
                          {{"timeslice_count", timeslice_count_},
                           {"component_count", component_count_},
                           {"microslice_count", microslice_count_},
                           {"content_bytes", content_bytes_},
                           {"timeslice_error_count", timeslice_error_count_},
                           {"component_error_count", component_error_count_},
                           {"microslice_error_count", microslice_error_count_},
                           {"timeslice_index", timeslice_index_}});
  }

  scheduler_.add([this] { report_status(); }, now + interval);
}
