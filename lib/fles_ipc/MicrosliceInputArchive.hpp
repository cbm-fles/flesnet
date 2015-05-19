// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "MicrosliceSource.hpp"
#include "ArchiveDescriptor.hpp"
#include "StorableMicroslice.hpp"
#include <string>
#include <fstream>
#include <memory>
#include <boost/archive/binary_iarchive.hpp>

namespace fles
{

//! The MicrosliceInputArchive deserializes microslice data sets from an input
// file.
class MicrosliceInputArchive : public MicrosliceSource
{
public:
    MicrosliceInputArchive(const std::string& filename);

    MicrosliceInputArchive(const MicrosliceInputArchive&) = delete;
    void operator=(const MicrosliceInputArchive&) = delete;

    virtual ~MicrosliceInputArchive(){};

    /// Read the next microslice.
    std::unique_ptr<StorableMicroslice> get()
    {
        return std::unique_ptr<StorableMicroslice>(do_get());
    };

    const ArchiveDescriptor& descriptor() const { return _descriptor; };

private:
    virtual StorableMicroslice* do_get();

    std::unique_ptr<std::ifstream> _ifstream;
    std::unique_ptr<boost::archive::binary_iarchive> _iarchive;
    ArchiveDescriptor _descriptor;
};

MicrosliceInputArchive::MicrosliceInputArchive(const std::string& filename)
{
    _ifstream = std::unique_ptr<std::ifstream>(
        new std::ifstream(filename.c_str(), std::ios::binary));
    if (!*_ifstream) {
        throw std::ios_base::failure("error opening file \"" + filename + "\"");
    }

    _iarchive = std::unique_ptr<boost::archive::binary_iarchive>(
        new boost::archive::binary_iarchive(*_ifstream));

    *_iarchive >> _descriptor;

    if (_descriptor.archive_type() !=
        ArchiveDescriptor::ArchiveType::MicrosliceArchive) {
        throw std::runtime_error(
            "File \"" + filename +
            "\" is not of type ArchiveType::MicrosliceArchive");
    }
}

StorableMicroslice* MicrosliceInputArchive::do_get()
{
    StorableMicroslice* sms = nullptr;
    try {
        sms = new StorableMicroslice();
        *_iarchive >> *sms;
    } catch (boost::archive::archive_exception e) {
        if (e.code == boost::archive::archive_exception::input_stream_error) {
            delete sms;
            return nullptr;
        }
        throw;
    }
    return sms;
}

} // namespace fles {
