// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::OutputArchive template class.
#pragma once

#include "ArchiveDescriptor.hpp"
#include "Sink.hpp"
#include <string>
#include <fstream>
#include <boost/archive/binary_oarchive.hpp>

namespace fles
{

/**
 * \brief The OutputArchive class serializes data sets to an output file.
 */
template <class Base, class Derived, ArchiveType archive_type>
class OutputArchive : public Sink<Base>
{
public:
    /**
     * \brief Construct an output archive object, open the given archive file
     * for writing, and write the archive descriptor.
     *
     * \param filename File name of the archive file
     */
    OutputArchive(const std::string& filename)
        : ofstream_(filename, std::ios::binary), oarchive_(ofstream_)
    {
        oarchive_ << descriptor_;
    }

    /// Delete copy constructor (non-copyable).
    OutputArchive(const OutputArchive&) = delete;
    /// Delete assignment operator (non-copyable).
    void operator=(const OutputArchive&) = delete;

    virtual ~OutputArchive(){};

    /// Store an item.
    virtual void put(const Base& item) override { do_put(item); }

    /// Deprecated alternative to put().
    void write(const Base& item) { put(item); }

private:
    std::ofstream ofstream_;
    boost::archive::binary_oarchive oarchive_;
    ArchiveDescriptor descriptor_{archive_type};

    void do_put(const Derived& item) { oarchive_ << item; }
};

} // namespace fles {
