// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Microslice.hpp"
#include "MicrosliceDescriptor.hpp"
#include "Sink.hpp"
#include "interface.h" // crcutil_interface
#include <memory>
#include <ostream>
#include <string>

class PatternChecker;

class MicrosliceAnalyzer : public fles::MicrosliceSink {
public:
  MicrosliceAnalyzer(uint64_t arg_output_interval,
                     size_t arg_out_verbosity,
                     std::ostream& arg_out,
                     std::string arg_output_prefix,
                     size_t component = 0);
  ~MicrosliceAnalyzer() override;

  void put(std::shared_ptr<const fles::Microslice> ms) override;

private:
  bool check_microslice(const fles::Microslice& ms);

  [[nodiscard]] std::string statistics() const;

  [[nodiscard]] uint32_t compute_crc(const fles::Microslice& ms) const;

  [[nodiscard]] bool check_crc(const fles::Microslice& ms) const;

  void initialize(const fles::Microslice& ms);

  crcutil_interface::CRC* crc32_engine_ = nullptr;

  fles::MicrosliceDescriptor reference_descriptor_{};
  std::unique_ptr<PatternChecker> pattern_checker_;

  uint64_t output_interval_ = UINT64_MAX;
  size_t out_verbosity_;
  std::ostream& out_;
  std::string output_prefix_;
  size_t component_;

  uint64_t reference_delta_t_ = 0;
  uint64_t previous_start_ = 0;

  size_t microslice_count_ = 0;
  size_t microslice_error_count_ = 0;
  size_t microslice_truncated_count_ = 0;
  size_t content_bytes_ = 0;
};
