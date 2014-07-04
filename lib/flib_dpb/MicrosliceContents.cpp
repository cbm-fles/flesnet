//! \author Michael Krieger

#include "MicrosliceContents.hpp"

namespace flib_dpb {

static const size_t DESC_OFFSET {16};

// extract DTM locations/sizes from a microslice in "packed DTM" format
static std::vector<DTM> get_dtms(const uint8_t *ms_data, size_t ms_size)
{
    ms_data += DESC_OFFSET;
    ms_size -= DESC_OFFSET;
    auto data = reinterpret_cast<const uint16_t*>(ms_data);
    auto size = ms_size*sizeof(*ms_data)/sizeof(*data);

    std::vector<DTM> dtms;
    auto end = data + size;
    while (data < end) {
        size_t i {0};
        size_t len ((data[i++] & 0xFF) + 1); // () -> explicit conversion
        if (len > 1) {
            dtms.push_back(DTM {data+i, len});
            i += len;
        }
        i += (~i & 3) + 1; // skip padding (i -> k*4)
        data += i;
    }
    return dtms;
}

MicrosliceContents::MicrosliceContents(const uint8_t *data, size_t size)
: _dtms {get_dtms(data, size)} {}

} // namespace
