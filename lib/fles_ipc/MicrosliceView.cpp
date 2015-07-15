// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "MicrosliceView.hpp"

namespace fles
{

MicrosliceView::MicrosliceView(MicrosliceDescriptor& d, uint8_t* content)
    : Microslice(&d, content)
{
}

} // namespace fles {
