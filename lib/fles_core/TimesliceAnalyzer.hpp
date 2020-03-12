// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "MicrosliceDescriptor.hpp"
#include "Sink.hpp"
#include "Timeslice.hpp"
#include "interface.h" // crcutil_interface
#include <memory>
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
  bool check_timeslice(const fles::Timeslice& ts);

  std::string statistics() const;
  void reset() {
    microslice_count_ = 0;
    content_bytes_ = 0;
  }

  uint32_t compute_crc(const fles::MicrosliceView& m) const;

  bool check_crc(const fles::MicrosliceView& m) const;

  bool check_microslice(const fles::MicrosliceView& m,
                        size_t component,
                        size_t microslice);

  void initialize(const fles::Timeslice& ts);

  crcutil_interface::CRC* crc32_engine_ = nullptr;

  std::vector<fles::MicrosliceDescriptor> reference_descriptors_;
  std::vector<std::unique_ptr<PatternChecker>> pattern_checkers_;

  uint64_t output_interval_ = UINT64_MAX;
  std::ostream& out_;
  std::string output_prefix_;
  std::ostream* hist_;

  size_t timeslice_count_ = 0;
  size_t timeslice_error_count_ = 0;
  size_t microslice_count_ = 0;
  size_t content_bytes_ = 0;
};
