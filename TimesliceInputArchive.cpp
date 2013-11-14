// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceInputArchive.hpp"

namespace fles
{

TimesliceInputArchive::TimesliceInputArchive(const std::string& filename)
    : _ifstream(filename.c_str(), std::ios::binary), _iarchive(_ifstream)
{
    _iarchive >> _descriptor;
}

std::unique_ptr<StorableTimeslice> TimesliceInputArchive::read()
{
    std::unique_ptr<StorableTimeslice> sts;
    try
    {
        sts = std::unique_ptr<StorableTimeslice>(new StorableTimeslice());
        _iarchive >> (*sts);
    }
    catch (boost::archive::archive_exception e)
    {
        return nullptr;
    }
    return sts;
}

} // namespace fles {
