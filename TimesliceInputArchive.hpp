// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "TimesliceSource.hpp"
#include "TimesliceArchiveDescriptor.hpp"
#include "StorableTimeslice.hpp"
#include <string>
#include <fstream>
#include <memory>
#include <boost/archive/binary_iarchive.hpp>

namespace fles
{

//! The TimesliceInputArchive deserializes timeslice data sets from an input
// file.
class TimesliceInputArchive : public TimesliceSource
{
public:
    TimesliceInputArchive(const std::string& filename);

    TimesliceInputArchive(const TimesliceInputArchive&) = delete;
    void operator=(const TimesliceInputArchive&) = delete;

    virtual ~TimesliceInputArchive() {};

    /// Read the next timeslice.
    std::unique_ptr<StorableTimeslice> get()
    {
        return std::unique_ptr<StorableTimeslice>(do_get());
    };

private:
    virtual StorableTimeslice* do_get();

    std::ifstream _ifstream;
    boost::archive::binary_iarchive _iarchive;
    TimesliceArchiveDescriptor _descriptor;
};

} // namespace fles {
