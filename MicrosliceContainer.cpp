#include "MicrosliceContainer.hpp"

namespace fles {

MicrosliceContainer::MicrosliceContainer(const MicrosliceView& mc)
: MicrosliceContainer {mc.desc(), mc.content()}
{
}

MicrosliceContainer::MicrosliceContainer(MicrosliceDescriptor d,
                                         const uint8_t *content_p)
: _desc (d), // cannot use {}, see http://stackoverflow.com/q/19347004
  _content {content_p, content_p + d.size}
{
}

MicrosliceContainer::MicrosliceContainer(MicrosliceDescriptor d,
                                         std::vector<uint8_t> content_v)
: desc (d),
  _content {std::move(content_v)}
{
    _desc.size = _content.size();
}

}
