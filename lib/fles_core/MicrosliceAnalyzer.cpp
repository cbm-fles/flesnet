// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "MicrosliceAnalyzer.hpp"
#include "PatternChecker.hpp"
#include <sstream>
#include <cassert>

MicrosliceAnalyzer::MicrosliceAnalyzer(uint64_t arg_output_interval,
                                       std::ostream& arg_out,
                                       std::string arg_output_prefix)
    : output_interval_(arg_output_interval), out_(arg_out),
      output_prefix_(arg_output_prefix)
{
    // create CRC-32C engine (Castagnoli polynomial)
    crc32_engine_ = crcutil_interface::CRC::Create(
        0x82f63b78, 0, 32, true, 0, 0, 0,
        crcutil_interface::CRC::IsSSE42Available(), NULL);
}

MicrosliceAnalyzer::~MicrosliceAnalyzer()
{
    if (crc32_engine_) {
        crc32_engine_->Delete();
    }
}

uint32_t MicrosliceAnalyzer::compute_crc(const fles::Microslice& ms) const
{
    assert(crc32_engine_);

    crcutil_interface::UINT64 crc64 = 0;
    crc32_engine_->Compute(ms.content(), ms.desc().size, &crc64);

    return static_cast<uint32_t>(crc64);
}

bool MicrosliceAnalyzer::check_crc(const fles::Microslice& ms) const
{
    return compute_crc(ms) == ms.desc().crc;
}

void MicrosliceAnalyzer::initialize(const fles::Microslice& ms)
{
    fles::MicrosliceDescriptor desc = ms.desc();
    reference_descriptor_ = desc;
    pattern_checker_ = PatternChecker::create(desc.sys_id, desc.sys_ver, 0);
}

bool MicrosliceAnalyzer::check_microslice(const fles::Microslice& ms)
{
    if (microslice_count_ == 0) {
        initialize(ms);
    }

    ++microslice_count_;
    content_bytes_ += ms.desc().size;

    if (ms.desc().flags &
            static_cast<uint16_t>(fles::MicrosliceFlags::CrcValid) &&
        check_crc(ms) == false) {
        out_ << output_prefix_ << "crc failure in microslice " << ms.desc().idx
             << std::endl;
        return false;
    }

    if (!pattern_checker_->check(ms)) {
        out_ << output_prefix_ << "pattern error in microslice "
             << ms.desc().idx << std::endl;
        return false;
    }

    return true;
}

std::string MicrosliceAnalyzer::statistics() const
{
    std::stringstream s;
    s << "microslices checked: " << microslice_count_ << " (" << content_bytes_
      << " bytes)";
    return s.str();
}

void MicrosliceAnalyzer::put(const fles::Microslice& ms)
{
    check_microslice(ms);
    if ((microslice_count_ % output_interval_) == 0) {
        out_ << output_prefix_ << statistics() << std::endl;
    }
}
