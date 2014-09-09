#include "StorableMicroslice.hpp"

#include "MicrosliceView.hpp"

namespace fles {

StorableMicroslice::StorableMicroslice(const MicrosliceView& mc)
: StorableMicroslice {mc.desc(), mc.content()}
{
}

StorableMicroslice::StorableMicroslice(MicrosliceDescriptor d,
                                       const uint8_t *content_p)
: _desc (d), // cannot use {}, see http://stackoverflow.com/q/19347004
  _content {content_p, content_p + d.size}
{
}

StorableMicroslice::StorableMicroslice(MicrosliceDescriptor d,
                                       std::vector<uint8_t> content_v)
: _desc (d),
  _content {std::move(content_v)}
{
    _desc.size = _content.size();
}

}
