//! \author Michael Krieger

#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace fles {

struct DTM {
    const uint16_t *data;
    size_t size;
};

struct MicrosliceContents {
    MicrosliceContents(const uint16_t *data, size_t size);
    // TODO construct from MicrosliceDescriptor (but it lacks the data)
    const std::vector<DTM>& dtms() const { return _dtms; };
private:
    std::vector<DTM> _dtms;
};

} // namespace
