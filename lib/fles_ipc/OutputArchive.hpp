// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ArchiveDescriptor.hpp"
#include <string>
#include <fstream>
#include <boost/archive/binary_oarchive.hpp>

namespace fles
{

/**
 * @brief The OutputArchive class serializes data sets to an output file.
 */
template <class T, ArchiveType archive_type> class OutputArchive
{
public:
    OutputArchive(const std::string& filename)
        : _ofstream(filename, std::ios::binary), _oarchive(_ofstream)
    {
        _oarchive << _descriptor;
    }

    OutputArchive(const OutputArchive&) = delete;
    void operator=(const OutputArchive&) = delete;

    /// Store an item.
    void write(const T& item) { _oarchive << item; }

private:
    std::ofstream _ofstream;
    boost::archive::binary_oarchive _oarchive;
    ArchiveDescriptor _descriptor{archive_type};
};

} // namespace fles {
