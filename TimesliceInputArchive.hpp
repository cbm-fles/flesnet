// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "TimesliceArchiveDescriptor.hpp"
#include "StorableTimeslice.hpp"
#include <string>
#include <fstream>
#include <memory>
#include <boost/archive/binary_iarchive.hpp>

namespace fles
{

//! The TimesliceInputArchive...
class TimesliceInputArchive
{
public:
    TimesliceInputArchive(const std::string& filename);

    TimesliceInputArchive(const TimesliceInputArchive&) = delete;
    void operator=(const TimesliceInputArchive&) = delete;

    /// Read the next timeslice.
    std::unique_ptr<StorableTimeslice> read();

private:
    std::ifstream _ifstream;
    boost::archive::binary_iarchive _iarchive;
    TimesliceArchiveDescriptor _descriptor;
};

} // namespace fles {
