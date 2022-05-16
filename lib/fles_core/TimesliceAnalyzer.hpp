// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "MicrosliceDescriptor.hpp"
#include "Scheduler.hpp"
#include "Sink.hpp"
#include "Timeslice.hpp"
#include "interface.h" // crcutil_interface
#include <chrono>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#define _TURN_OFF_PLATFORM_STRING
#include <cpprest/http_client.h>

class PatternChecker;

class TimesliceAnalyzer : public fles::TimesliceSink {
public:
  TimesliceAnalyzer(uint64_t arg_output_interval,
                    std::ostream& arg_out,
                    std::string arg_output_prefix,
                    std::ostream* arg_hist,
                    const std::string& monitor_uri);
  ~TimesliceAnalyzer() override;

  void put(std::shared_ptr<const fles::Timeslice> timeslice) override;

private:
  void initialize(const fles::Timeslice& ts);
  void reset() {
    microslice_count_ = 0;
    content_bytes_ = 0;
  }

  [[nodiscard]] bool check_timeslice(const fles::Timeslice& ts);
  [[nodiscard]] bool check_component(const fles::Timeslice& ts, size_t c);
  [[nodiscard]] bool
  check_microslice(const fles::Timeslice& ts, size_t c, size_t m);

  [[nodiscard]] uint32_t compute_crc(const fles::MicrosliceView& m) const;
  [[nodiscard]] bool check_crc(const fles::MicrosliceView& m) const;

  void print(std::string text, std::string prefix = "");
  void print_reference();
  void
  print_microslice_descriptor(const fles::Timeslice& ts, size_t c, size_t m);
  void print_microslice_content(const fles::Timeslice& ts, size_t c, size_t m);

  [[nodiscard]] std::string statistics() const;

  [[nodiscard]] std::string
  location_string(size_t ts,
                  std::optional<size_t> c = std::nullopt,
                  std::optional<size_t> m = std::nullopt) const;

  [[nodiscard]] bool output_active() const;

  crcutil_interface::CRC* crc32_engine_ = nullptr;

  std::vector<fles::MicrosliceDescriptor> reference_descriptors_;
  std::vector<std::unique_ptr<PatternChecker>> pattern_checkers_;

  uint64_t output_interval_ = UINT64_MAX;
  std::ostream& out_;
  std::string output_prefix_;
  std::ostream* hist_;
  std::chrono::system_clock::time_point previous_output_time_;
  static constexpr std::chrono::seconds output_time_interval_{1};

  size_t timeslice_count_ = 0;
  size_t timeslice_error_count_ = 0;
  size_t component_count_ = 0;
  size_t component_error_count_ = 0;
  size_t microslice_count_ = 0;
  size_t microslice_error_count_ = 0;
  size_t content_bytes_ = 0;

  void report_status();

  std::unique_ptr<web::http::client::http_client> monitor_client_;
  std::unique_ptr<pplx::task<void>> monitor_task_;
  std::string hostname_;

  Scheduler scheduler_;
};
