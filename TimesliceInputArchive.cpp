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
    StorableTimeslice s;
    try
    {
        _iarchive >> s;
    }
    catch (boost::archive::archive_exception e)
    {
        return nullptr;
    }
    std::unique_ptr<StorableTimeslice> sts = std::unique_ptr
        <StorableTimeslice>(new StorableTimeslice(s));
    return sts;
}

} // namespace fles {
