// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceInputArchive.hpp"

namespace fles
{

TimesliceInputArchive::TimesliceInputArchive(const std::string& filename)
    : _ifstream(filename.c_str(), std::ios::binary), _iarchive(_ifstream)
{
    _iarchive >> _descriptor;
}

StorableTimeslice* TimesliceInputArchive::do_get()
{
    StorableTimeslice* sts = nullptr;
    try
    {
        sts = new StorableTimeslice();
        _iarchive >> *sts;
    }
    catch (boost::archive::archive_exception e)
    {
        delete sts;
        return nullptr;
    }
    return sts;
}

} // namespace fles {
