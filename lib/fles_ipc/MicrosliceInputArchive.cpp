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
    StorableMicroslice* sms = nullptr;
    try {
        sms = new StorableMicroslice();
        _iarchive >> *sms;
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
