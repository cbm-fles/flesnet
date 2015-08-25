// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Microslice.hpp"
#include "MicrosliceDescriptor.hpp"
#include "Sink.hpp"
#include "interface.h" // crcutil_interface
#include <ostream>
#include <string>

class PatternChecker;

class MicrosliceAnalyzer : public fles::MicrosliceSink
{
public:
    MicrosliceAnalyzer(uint64_t arg_output_interval, std::ostream& arg_out,
                       std::string arg_output_prefix);
    ~MicrosliceAnalyzer();

    virtual void put(const fles::Microslice& Microslice) override;

private:
    bool check_microslice(const fles::Microslice& ms);

    std::string statistics() const;

    uint32_t compute_crc(const fles::Microslice& ms) const;

    bool check_crc(const fles::Microslice& ms) const;

    void initialize(const fles::Microslice& ms);

    crcutil_interface::CRC* crc32_engine_ = nullptr;

    fles::MicrosliceDescriptor reference_descriptor_;
    std::unique_ptr<PatternChecker> pattern_checker_;

    uint64_t output_interval_ = UINT64_MAX;
    std::ostream& out_;
    std::string output_prefix_;

    size_t microslice_count_ = 0;
    size_t content_bytes_ = 0;
};
