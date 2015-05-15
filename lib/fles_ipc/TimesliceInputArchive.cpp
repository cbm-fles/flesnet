// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceInputArchive.hpp"

namespace fles
{

TimesliceInputArchive::TimesliceInputArchive(const std::string& filename)
{
    _ifstream = std::unique_ptr<std::ifstream>(
        new std::ifstream(filename.c_str(), std::ios::binary));

    _iarchive = std::unique_ptr<boost::archive::binary_iarchive>(
        new boost::archive::binary_iarchive(*_ifstream));

    *_iarchive >> _descriptor;
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
