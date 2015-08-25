// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Timeslice.hpp"
#include "MicrosliceDescriptor.hpp"
#include "Sink.hpp"
#include "interface.h" // crcutil_interface
#include <ostream>
#include <string>

class PatternChecker;

class TimesliceAnalyzer : public fles::TimesliceSink
{
public:
    TimesliceAnalyzer(uint64_t arg_output_interval, std::ostream& arg_out,
                      std::string arg_output_prefix);
    ~TimesliceAnalyzer();

    virtual void put(const fles::Timeslice& timeslice) override;

private:
    bool check_timeslice(const fles::Timeslice& ts);

    std::string statistics() const;
    void reset()
    {
        microslice_count_ = 0;
        content_bytes_ = 0;
    }

    uint32_t compute_crc(const fles::MicrosliceView m) const;

    bool check_crc(const fles::MicrosliceView m) const;

    bool check_microslice(const fles::MicrosliceView m, size_t component,
                          size_t microslice);

    void initialize(const fles::Timeslice& ts);

    crcutil_interface::CRC* crc32_engine_ = nullptr;

    std::vector<fles::MicrosliceDescriptor> reference_descriptors_;
    std::vector<std::unique_ptr<PatternChecker>> pattern_checkers_;

    uint64_t output_interval_ = UINT64_MAX;
    std::ostream& out_;
    std::string output_prefix_;

    size_t timeslice_count_ = 0;
    size_t microslice_count_ = 0;
    size_t content_bytes_ = 0;
};

class PatternChecker
{
public:
    virtual ~PatternChecker(){};

    virtual bool check(const fles::MicrosliceView m) = 0;
    virtual void reset(){};

    static std::unique_ptr<PatternChecker>
    create(uint8_t arg_sys_id, uint8_t arg_sys_ver, size_t component);
};

class FlesnetPatternChecker : public PatternChecker
{
public:
    FlesnetPatternChecker(std::size_t arg_component)
        : component(arg_component){};

    virtual bool check(const fles::MicrosliceView m) override;

private:
    std::size_t component = 0;
};

class FlibLegacyPatternChecker : public PatternChecker
{
public:
    virtual bool check(const fles::MicrosliceView m) override;
    virtual void reset() override
    {
        frame_number_ = 0;
        pgen_sequence_number_ = 0;
    };

private:
    bool check_cbmnet_frames(const uint16_t* content, size_t size,
                             uint8_t sys_id, uint8_t sys_ver);
    bool check_content_pgen(const uint16_t* content, size_t size);

    uint8_t frame_number_ = 0;
    uint16_t pgen_sequence_number_ = 0;
};

class FlibPatternChecker : public PatternChecker
{
public:
    virtual bool check(const fles::MicrosliceView m) override;
    virtual void reset() override { flib_pgen_packet_number_ = 0; };

private:
    uint32_t flib_pgen_packet_number_ = 0;
};

class GenericPatternChecker : public PatternChecker
{
public:
    virtual bool check(const fles::MicrosliceView /* m */) override
    {
        return true;
    };
};
