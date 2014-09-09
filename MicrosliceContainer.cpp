#include "MicrosliceContainer.hpp"

namespace fles {

MicrosliceContainer::MicrosliceContainer(MicrosliceDescriptor d,
                                         const uint8_t *content_p)
: desc (d), // cannot use {}, see http://stackoverflow.com/q/19347004
  content {content_p},
  _content {}
{
}

MicrosliceContainer::MicrosliceContainer(MicrosliceDescriptor d,
                                         std::vector<uint8_t> content_v)
: desc (d),
  content {content_v.data()}, // pointer remains valid as the vector is moved
  _content {std::move(content_v)}
{
    desc.size = _content.size();
}

}
