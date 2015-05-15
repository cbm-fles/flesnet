// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "MicrosliceView.hpp"
#include "StorableMicroslice.hpp"

namespace fles
{

StorableMicroslice::StorableMicroslice(const StorableMicroslice& ms)
    : _desc(ms._desc), _content(ms._content)
{
    init_pointers();
}

StorableMicroslice::StorableMicroslice(StorableMicroslice&& ms)
    : _desc(std::move(ms._desc)), _content(std::move(ms._content))
{
    init_pointers();
}

StorableMicroslice::StorableMicroslice(const Microslice& ms)
    : StorableMicroslice{ms.desc(), ms.content()}
{
}

StorableMicroslice::StorableMicroslice(MicrosliceDescriptor d,
                                       const uint8_t* content_p)
    : _desc(d), // cannot use {}, see http://stackoverflow.com/q/19347004
      _content{content_p, content_p + d.size}
{
    init_pointers();
}

StorableMicroslice::StorableMicroslice(MicrosliceDescriptor d,
                                       std::vector<uint8_t> content_v)
    : _desc(d), _content{std::move(content_v)}
{
    _desc.size = static_cast<uint32_t>(_content.size());
    init_pointers();
}

StorableMicroslice::StorableMicroslice() {}

} // namespace fles {
