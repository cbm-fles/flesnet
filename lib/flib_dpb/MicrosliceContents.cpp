//! \author Michael Krieger

#include "MicrosliceContents.hpp"

namespace flib_dpb {

static const size_t DESC_OFFSET {16};
static std::vector<DTM> get_dtms(const uint8_t *ms_data, size_t ms_size);
static size_t next_dtm(std::vector<DTM>& dtms, const uint16_t *data);

//! extract all DTMs from a microslice in "packed DTM" format
std::vector<DTM> get_dtms(const uint8_t *ms_data, size_t ms_size)
{
    ms_data += DESC_OFFSET;
    ms_size -= DESC_OFFSET;
    auto data = reinterpret_cast<const uint16_t*>(ms_data);
    auto size = ms_size*sizeof(*ms_data)/sizeof(*data);

    std::vector<DTM> dtms;
    auto end = data + size;
    while (data < end) {
        data += next_dtm(dtms, data);
    }
    return dtms;
}

//! extract location/size of a single DTM contained in a microslice
size_t next_dtm(std::vector<DTM>& dtms, const uint16_t *data)
{
    size_t n {1};
    size_t size {(*data++ & 0xFFu)};
    if (size++) {
        dtms.push_back(DTM {data, size});
        n += size;
    }
    n += (~n + 1) % 4; // skip padding (n -> k*4)
    return n;
}

MicrosliceContents::MicrosliceContents(const uint8_t *data, size_t size)
: _dtms {get_dtms(data, size)} {}

} // namespace
