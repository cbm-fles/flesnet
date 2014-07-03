//! \author Michael Krieger

#include "MicrosliceContents.hpp"

namespace fles {

// extract pointers to DTMs from a microslice
std::vector<DTM> init_dtms(const uint16_t *data, size_t size)
{
    std::vector<DTM> v;
    const uint16_t *end = data + size;
    while (data < end) {
        size_t i = 0;
        size_t len = (data[i++] & 0xFF) + 1;
        if (len > 1) {
            v.push_back(DTM {data+i, len});
            i += len;
        }
        i += (~i & 3) + 1; // skip padding (i -> k*4)
        data += i;
    }
    return v;
}

MicrosliceContents::MicrosliceContents(const uint16_t *data, size_t size)
: _dtms(init_dtms(data, size)) {}

} // namespace
