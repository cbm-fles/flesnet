// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceInputArchive.hpp"

namespace fles
{

TimesliceInputArchive::TimesliceInputArchive(const std::string& filename)
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
        ArchiveDescriptor::ArchiveType::TimesliceArchive) {
        throw std::runtime_error(
            "File \"" + filename +
            "\" is not of type ArchiveType::TimesliceArchive");
    }
}

StorableTimeslice* TimesliceInputArchive::do_get()
{
    StorableTimeslice* sts = nullptr;
    try {
        sts = new StorableTimeslice();
        *_iarchive >> *sts;
    } catch (boost::archive::archive_exception e) {
        if (e.code == boost::archive::archive_exception::input_stream_error) {
            delete sts;
            return nullptr;
        }
        throw;
    }
    return sts;
}

} // namespace fles {
