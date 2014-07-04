//! \author Michael Krieger

#include "MicrosliceContents.hpp"

namespace fles {

// extract pointers to DTMs from a microslice
static std::vector<DTM> get_dtms(const uint16_t *data, size_t size)
{
    std::vector<DTM> dtms;
    const uint16_t *end = data + size;
    while (data < end) {
        size_t i = 0;
        size_t len = (data[i++] & 0xFF) + 1;
        if (len > 1) {
            dtms.push_back(DTM {data+i, len});
            i += len;
        }
        i += (~i & 3) + 1; // skip padding (i -> k*4)
        data += i;
    }
    return dtms;
}

MicrosliceContents::MicrosliceContents(const uint16_t *data, size_t size)
: _dtms(get_dtms(data, size)) {}

} // namespace
