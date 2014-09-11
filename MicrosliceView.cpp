#include "MicrosliceView.hpp"

namespace fles {

MicrosliceView::MicrosliceView(MicrosliceDescriptor d, const uint8_t *content)
: _desc (d), // cannot use {}, see http://stackoverflow.com/q/19347004
  _content {content}
{
}

}
