// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ArchiveDescriptor.hpp"
#include "StorableMicroslice.hpp"
#include <string>
#include <fstream>
#include <boost/archive/binary_oarchive.hpp>

namespace fles
{

//! The MicrosliceOutputArchive serializes microslice data sets to an output
// file.
class MicrosliceOutputArchive
{
public:
    MicrosliceOutputArchive(const std::string& filename)
        : _ofstream(filename, std::ios::binary), _oarchive(_ofstream)
    {
        _oarchive << _descriptor;
    }

    MicrosliceOutputArchive(const MicrosliceOutputArchive&) = delete;
    void operator=(const MicrosliceOutputArchive&) = delete;

    /// Store a microslice.
    void write(const StorableMicroslice& microslice)
    {
        _oarchive << microslice;
    }

private:
    std::ofstream _ofstream;
    boost::archive::binary_oarchive _oarchive;
    ArchiveDescriptor _descriptor{
        ArchiveDescriptor::ArchiveType::MicrosliceArchive};
};

} // namespace fles {
