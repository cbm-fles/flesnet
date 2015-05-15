// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "MicrosliceView.hpp"

namespace fles
{

MicrosliceView::MicrosliceView(const MicrosliceDescriptor& d,
                               const uint8_t* content)
    : Microslice(&d, content)
{
}

} // namespace fles {
