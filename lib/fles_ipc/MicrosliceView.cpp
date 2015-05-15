#include "MicrosliceView.hpp"

namespace fles
{

MicrosliceView::MicrosliceView(MicrosliceDescriptor& d, const uint8_t *content)
    : _desc_ptr(*d),
      _content_ptr(content)
{
}
}
