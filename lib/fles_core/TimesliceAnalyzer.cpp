// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceAnalyzer.hpp"
#include "PatternChecker.hpp"
#include "TimesliceDebugger.hpp"
#include "Utility.hpp"
#include <cassert>
#include <sstream>

TimesliceAnalyzer::TimesliceAnalyzer(uint64_t arg_output_interval,
                                     std::ostream& arg_out,
                                     std::string arg_output_prefix)
    : output_interval_(arg_output_interval), out_(arg_out),
      output_prefix_(std::move(arg_output_prefix))
{
    // create CRC-32C engine (Castagnoli polynomial)
    crc32_engine_ = crcutil_interface::CRC::Create(
        0x82f63b78, 0, 32, true, 0, 0, 0,
        crcutil_interface::CRC::IsSSE42Available(), NULL);
}

TimesliceAnalyzer::~TimesliceAnalyzer()
{
    if (crc32_engine_ != nullptr) {
        crc32_engine_->Delete();
    }
}

uint32_t TimesliceAnalyzer::compute_crc(const fles::MicrosliceView m) const
{
    assert(crc32_engine_);

    crcutil_interface::UINT64 crc64 = 0;
    crc32_engine_->Compute(m.content(), m.desc().size, &crc64);

    return static_cast<uint32_t>(crc64);
}

bool TimesliceAnalyzer::check_crc(const fles::MicrosliceView m) const
{
    return compute_crc(m) == m.desc().crc;
}

bool TimesliceAnalyzer::check_microslice(const fles::MicrosliceView m,
                                         size_t component, size_t microslice)
{
// disabled, not applicable when using start time instead of index
#if 0
    if (m.desc().idx != microslice) {
        out_ << "microslice index " << m.desc().idx << " found in m.desc() "
             << microslice << std::endl;
        return false;
    }
#endif

    ++microslice_count_;
    content_bytes_ += m.desc().size;

    if ((m.desc().flags &
         static_cast<uint16_t>(fles::MicrosliceFlags::OverflowFlim)) != 0) {
        out_ << output_prefix_ << " microslice " << microslice
             << " truncated by FLIM" << std::endl;
    }

    if (!pattern_checkers_.at(component)->check(m)) {
        return false;
    }

    if (((m.desc().flags &
          static_cast<uint16_t>(fles::MicrosliceFlags::CrcValid)) != 0) &&
        !check_crc(m)) {
        out_ << "crc failure in microslice " << microslice << std::endl;
        return false;
    }

    return true;
}

void TimesliceAnalyzer::initialize(const fles::Timeslice& ts)
{
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

bool TimesliceAnalyzer::check_timeslice(const fles::Timeslice& ts)
{
    if (timeslice_count_ == 0) {
        initialize(ts);
    }

    ++timeslice_count_;

    if (ts.num_components() == 0) {
        out_ << "no component in timeslice " << ts.index() << std::endl;
        ++timeslice_error_count_;
        return false;
    }

    for (size_t c = 0; c < ts.num_components(); ++c) {
        if (ts.num_microslices(c) == 0) {
            out_ << "no microslices in timeslice " << ts.index()
                 << ", component " << c << std::endl;
            ++timeslice_error_count_;
            return false;
        }
        pattern_checkers_.at(c)->reset();
        for (size_t m = 0; m < ts.num_microslices(c); ++m) {
            bool success =
                check_microslice(ts.get_microslice(c, m), c,
                                 ts.index() * ts.num_core_microslices() + m);
            if (!success) {
                out_ << "pattern error in timeslice " << ts.index()
                     << ", microslice " << m << ", component " << c
                     << std::endl;
                if (timeslice_error_count_ == 0) { // full dump for first error
                    out_ << "microslice content:\n"
                         << MicrosliceDescriptorDump(
                                ts.get_microslice(c, m).desc())
                         << BufferDump(ts.get_microslice(c, m).content(),
                                       ts.get_microslice(c, m).desc().size);
                }
                ++timeslice_error_count_;
                return false;
            }
        }
    }
    return true;
}

std::string TimesliceAnalyzer::statistics() const
{
    std::stringstream s;
    s << "timeslices checked: " << timeslice_count_ << " ("
      << human_readable_count(content_bytes_) << " in " << microslice_count_
      << " microslices, avg: "
      << static_cast<double>(content_bytes_) / microslice_count_ << ")";
    if (timeslice_error_count_ > 0) {
        s << " [" << timeslice_error_count_ << " errors]";
    }
    return s.str();
}

void TimesliceAnalyzer::put(std::shared_ptr<const fles::Timeslice> timeslice)
{
    check_timeslice(*timeslice);
    if ((timeslice_count_ % output_interval_) == 0) {
        out_ << output_prefix_ << statistics() << std::endl;
        reset();
    }
}
