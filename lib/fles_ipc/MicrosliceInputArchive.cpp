// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "MicrosliceInputArchive.hpp"

namespace fles
{

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
