// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "MicrosliceInputArchive.hpp"

namespace fles
{

MicrosliceInputArchive::MicrosliceInputArchive(const std::string& filename)
    : _ifstream(filename.c_str(), std::ios::binary), _iarchive(_ifstream)
{
    _iarchive >> _descriptor;
}

StorableMicroslice* MicrosliceInputArchive::do_get()
{
    StorableMicroslice* sts = nullptr;
    try
    {
        sts = new StorableMicroslice();
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
