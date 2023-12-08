// Copyright 2013, 2015, 2021, 2023 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceAnalyzer.hpp"
#include "PatternChecker.hpp"
#include "System.hpp"
#include "TimesliceDebugger.hpp"
#include "Utility.hpp"
#include "log.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <cassert>
#include <chrono>
#include <iomanip>
#include <sstream>

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
} // namespace

TimesliceAnalyzer::TimesliceAnalyzer(uint64_t arg_output_interval,
                                     std::ostream& arg_out,
                                     std::string arg_output_prefix,
                                     std::ostream* arg_hist,
                                     cbm::Monitor* monitor)
    : output_interval_(arg_output_interval), out_(arg_out),
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
    print(statistics(), "* ");
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
        PatternChecker::create(desc.sys_id, desc.sys_ver, c));
  }
}

bool TimesliceAnalyzer::check_timeslice(const fles::Timeslice& ts) {
  if (timeslice_count_ == 0) {
    initialize(ts);
    print_reference();
    // print timeslice duration
    if (ts.num_core_microslices() > 1 && ts.num_components() > 0) {
      uint64_t ms_length = ts.descriptor(0, 1).idx - ts.descriptor(0, 0).idx;
      uint64_t ts_length = ts.num_core_microslices() * ms_length;
      print("ts duration: " + ns_to_string(ts_length) + " (" +
            std::to_string(ts.num_core_microslices()) + " * " +
            ns_to_string(ms_length) + ")");
    }
    // print start time
    print("start time: " + ns_timestamp(ts.start_time()));
  } else if (timeslice_count_ == 1) {
    // print timeslice start index and stride
    uint64_t stride = ts.index() - start_index_;
    print("start index: " + std::to_string(start_index_) +
          " (stride: " + std::to_string(stride) + ")");
  }

  ++timeslice_count_;
  timeslice_index_ = ts.index();

  // timeslice global checks
  if (ts.num_components() == 0) {
    if (output_active()) {
      auto location = location_string(ts.index());
      print("error in " + location + ": no component in timeslice");
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
      auto location = location_string(ts.index());
      print("error in " + location + ": start time mismatch");
      for (size_t c = 0; c < ts.num_components(); ++c) {
        print("  component " + std::to_string(c) + " start time: " +
              std::to_string(ts.get_microslice(c, 0).desc().idx));
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
      auto location = location_string(ts.index(), c);
      print("error in " + location + ": no microslices in component");
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
        auto location = location_string(ts.index(), c);
        print("error in " + location +
              ": start time not increasing in first two microslices");
        print_microslice_descriptor(ts, c, 0);
        print_microslice_descriptor(ts, c, 1);
      }
      component_success = false;
    } else {
      uint64_t reference_delta = second - first;
      for (size_t m = 2; m < ts.num_microslices(c); ++m) {
        uint64_t this_start_time = ts.get_microslice(c, m).desc().idx;
        uint64_t expected_start_time = first + m * reference_delta;
        if (this_start_time != expected_start_time) {
          if (output_active()) {
            auto location = location_string(ts.index(), c, m);
            print("error in " + location +
                  ": unexpected microslice start time");
            print_microslice_descriptor(ts, c, 0);
            print_microslice_descriptor(ts, c, 1);
            print_microslice_descriptor(ts, c, m);
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
      auto location = location_string(ts.index(), c, m);
      print("error in " + location +
            ": unknown header format in microslice descriptor");
      print_microslice_descriptor(ts, c, m);
    }
  }

  // check descriptor consistency
  auto r = reference_descriptors_.at(c);
  if (d.eq_id != r.eq_id || d.sys_id != r.sys_id || d.sys_ver != r.sys_ver) {
    error = true;
    if (output_active()) {
      auto location = location_string(ts.index(), c, m);
      print("error in " + location +
            ": unexpected change in microslice descriptor");
      print_microslice_descriptor(ts, c, m);
    }
  }

  bool truncated =
      (d.flags & static_cast<uint16_t>(fles::MicrosliceFlags::OverflowFlim)) !=
      0;
  if (truncated && output_active()) {
    auto location = location_string(ts.index(), c, m);
    print("error in " + location + ": microslice truncated by FLIM");
    print_microslice_descriptor(ts, c, m);
    print_microslice_content(ts, c, m);
  }

  bool pattern_error = !pattern_checkers_.at(c)->check(mv);
  if (pattern_error && output_active()) {
    auto location = location_string(ts.index(), c, m);
    print("error in " + location + ": pattern error");
    print_microslice_descriptor(ts, c, m);
    print_microslice_content(ts, c, m);
  }

  bool crc_error =
      ((d.flags & static_cast<uint16_t>(fles::MicrosliceFlags::CrcValid)) !=
       0) &&
      !check_crc(mv);
  if (crc_error && output_active()) {
    auto location = location_string(ts.index(), c, m);
    print("error in " + location + ": crc failure");
    print_microslice_descriptor(ts, c, m);
    print_microslice_content(ts, c, m);
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

void TimesliceAnalyzer::print(std::string text, std::string prefix) {
  if (text.back() == '\n') {
    text.erase(text.end() - 1);
  }
  std::vector<std::string> lines;
  boost::split(lines, text, [](char c) { return c == '\n'; });
  for (auto const& line : lines) {
    out_ << output_prefix_ << prefix << line << std::endl;
  }
}

void TimesliceAnalyzer::print_reference() {
  print("timeslice analyzer initialized with " +
        std::to_string(reference_descriptors_.size()) + " components");
  for (size_t c = 0; c < reference_descriptors_.size(); ++c) {
    const fles::MicrosliceDescriptor& md = reference_descriptors_.at(c);
    print("  component " + std::to_string(c) + ":" +
          boost::str(boost::format(" eq_id=%04x") %
                     static_cast<unsigned int>(md.eq_id)) +
          boost::str(boost::format(" sys=%02x-%02x") %
                     static_cast<unsigned int>(md.sys_id) %
                     static_cast<unsigned int>(md.sys_ver)) +
          " " + fles::to_string(static_cast<fles::Subsystem>(md.sys_id)));
  }
}

void TimesliceAnalyzer::print_microslice_descriptor(const fles::Timeslice& ts,
                                                    size_t c,
                                                    size_t m) {
  auto location = location_string(ts.index(), c, m);
  print("microslice descriptor of " + location + ":");
  print(boost::str(boost::format("%s") %
                   MicrosliceDescriptorDump(ts.get_microslice(c, m).desc())),
        "  ");
}

void TimesliceAnalyzer::print_microslice_content(const fles::Timeslice& ts,
                                                 size_t c,
                                                 size_t m) {
  auto location = location_string(ts.index(), c, m);
  print("microslice content of " + location + ":");
  print(boost::str(boost::format("%s") %
                   BufferDump(ts.get_microslice(c, m).content(),
                              ts.get_microslice(c, m).desc().size)),
        "  ");
}

std::string TimesliceAnalyzer::statistics() const {
  std::stringstream s;
  s << "checked " << timeslice_count_ << " ts, " << component_count_ << " c, "
    << microslice_count_ << " m, "
    << human_readable_count(content_bytes_, true);
  if (timeslice_error_count_ > 0) {
    s << " [with errors: " << timeslice_error_count_ << "ts/"
      << component_error_count_ << "c/" << microslice_error_count_ << "m]";
  }
  return s.str();
}

std::string TimesliceAnalyzer::location_string(size_t ts,
                                               std::optional<size_t> c,
                                               std::optional<size_t> m) const {
  if (c && m) {
    return boost::str(boost::format("ts%d/c%d/m%d") % ts % *c % *m);
  }
  if (c) {
    return boost::str(boost::format("ts%d/c%d") % ts % *c);
  }
  return boost::str(boost::format("ts%d") % ts);
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
