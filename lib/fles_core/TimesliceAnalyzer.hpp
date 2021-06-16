// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "MicrosliceDescriptor.hpp"
#include "Sink.hpp"
#include "Timeslice.hpp"
#include "interface.h" // crcutil_interface
#include <memory>
#include <optional>
#include <ostream>
#include <string>

class PatternChecker;

class TimesliceAnalyzer : public fles::TimesliceSink {
public:
  TimesliceAnalyzer(uint64_t arg_output_interval,
                    std::ostream& arg_out,
                    std::string arg_output_prefix,
                    std::ostream* arg_hist);
  ~TimesliceAnalyzer() override;

  void put(std::shared_ptr<const fles::Timeslice> timeslice) override;

private:
  void initialize(const fles::Timeslice& ts);
  void reset() {
    microslice_count_ = 0;
    content_bytes_ = 0;
  }

  [[nodiscard]] bool check_timeslice(const fles::Timeslice& ts);
  [[nodiscard]] bool check_component(const fles::Timeslice& ts,
                                     size_t component);
  [[nodiscard]] bool check_microslice(const fles::MicrosliceView& m,
                                      size_t component,
                                      size_t microslice);

  [[nodiscard]] uint32_t compute_crc(const fles::MicrosliceView& m) const;
  [[nodiscard]] bool check_crc(const fles::MicrosliceView& m) const;

  void print(std::string text, std::string prefix = "");
  void print_reference();
  void print_microslice_descriptor(const fles::Timeslice& ts,
                                   size_t component,
                                   size_t microslice);
  void print_microslice_content(const fles::Timeslice& ts,
                                size_t component,
                                size_t microslice);

  [[nodiscard]] std::string statistics() const;

  [[nodiscard]] std::string
  location_string(size_t timeslice,
                  std::optional<size_t> component = std::nullopt,
                  std::optional<size_t> microslice = std::nullopt) const;

  [[nodiscard]] bool output_active() const;

  crcutil_interface::CRC* crc32_engine_ = nullptr;

  std::vector<fles::MicrosliceDescriptor> reference_descriptors_;
  std::vector<std::unique_ptr<PatternChecker>> pattern_checkers_;

  uint64_t output_interval_ = UINT64_MAX;
  std::ostream& out_;
  std::string output_prefix_;
  std::ostream* hist_;

  size_t timeslice_count_ = 0;
  size_t timeslice_error_count_ = 0;
  size_t component_count_ = 0;
  size_t component_error_count_ = 0;
  size_t microslice_count_ = 0;
  size_t microslice_error_count_ = 0;
  size_t content_bytes_ = 0;
};
