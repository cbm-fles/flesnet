#include "StorableMicroslice.hpp"

#include "MicrosliceView.hpp"

namespace fles
{

StorableMicroslice::StorableMicroslice(const StorableMicroslice& ms)
    :  _content(ms._content),
       _desc(ms._desc),
       _content_ptr(_content.data()),
       _desc_ptr(&_desc)
{
}

StorableMicroslice::StorableMicroslice(StorableMicroslice&& ms)
    : _content(std::move(ms._data)),
      _desc(std::move(ms._desc)),
      _content_ptr(_content.data()),
      _desc_ptr(&_desc)
{
}

StorableMicroslice::StorableMicroslice(const Microslice& ms)
    : StorableMicroslice{ms.desc(), ms.content()}
{
}

StorableMicroslice::StorableMicroslice(MicrosliceDescriptor d,
                                       const uint8_t *content_p)
: _desc(d), // cannot use {}, see http://stackoverflow.com/q/19347004
  _content{content_p, content_p + d.size}
    _content_ptr(_content.data()),
      _desc_ptr(&_desc)
{
}

StorableMicroslice::StorableMicroslice(MicrosliceDescriptor d,
                                       std::vector<uint8_t> content_v)
: _desc(d),
  _content{std::move(content_v)}
    _content_ptr(_content.data()),
      _desc_ptr(&_desc)
{
    _desc.size = static_cast<uint32_t>(_content.size());
}
}
