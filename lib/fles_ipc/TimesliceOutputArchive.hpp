// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "TimesliceArchiveDescriptor.hpp"
#include "StorableTimeslice.hpp"
#include <string>
#include <fstream>
#include <boost/archive/binary_oarchive.hpp>

namespace fles
{

//! The TimesliceOutputArchive serializes timeslice data sets to an output file.
class TimesliceOutputArchive
{
public:
    TimesliceOutputArchive(const std::string& filename)
        : _ofstream(filename, std::ios::binary), _oarchive(_ofstream)
    {
        _oarchive << _descriptor;
    }

    TimesliceOutputArchive(const TimesliceOutputArchive&) = delete;
    void operator=(const TimesliceOutputArchive&) = delete;

    /// Store a timeslice.
    void write(const StorableTimeslice& timeslice) { _oarchive << timeslice; }

private:
    std::ofstream _ofstream;
    boost::archive::binary_oarchive _oarchive;
    TimesliceArchiveDescriptor _descriptor{true};
};

} // namespace fles {
