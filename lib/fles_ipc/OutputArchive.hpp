// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ArchiveDescriptor.hpp"
#include <string>
#include <fstream>
#include <boost/archive/binary_oarchive.hpp>

namespace fles
{

/**
 * \brief The OutputArchive class serializes data sets to an output file.
 */
template <class T, ArchiveType archive_type> class OutputArchive
{
public:
    /**
     * \brief Construct an output archive object, open the given archive file
     * for writing, and write the archive descriptor.
     *
     * \param filename File name of the archive file
     */
    OutputArchive(const std::string& filename)
        : _ofstream(filename, std::ios::binary), _oarchive(_ofstream)
    {
        _oarchive << _descriptor;
    }

    /// Delete copy constructor (non-copyable).
    OutputArchive(const OutputArchive&) = delete;
    /// Delete assignment operator (non-copyable).
    void operator=(const OutputArchive&) = delete;

    /// Store an item.
    void write(const T& item) { _oarchive << item; }

private:
    std::ofstream _ofstream;
    boost::archive::binary_oarchive _oarchive;
    ArchiveDescriptor _descriptor{archive_type};
};

} // namespace fles {
