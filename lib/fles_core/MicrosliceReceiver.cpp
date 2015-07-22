// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "MicrosliceReceiver.hpp"

namespace fles
{

MicrosliceReceiver::MicrosliceReceiver(DataSource& data_source)
    : _data_source(data_source)
{
}

MicrosliceView* MicrosliceReceiver::do_get()
{
    if (_eof)
        return nullptr;

    // TODO: add code here ;-)
    return nullptr;
}
}
