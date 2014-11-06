#pragma once

#include "MicrosliceDescriptor.hpp"

namespace fles {

/**
 * Provide read access to microslice metadata and content stored elsewhere
 * (for example in an already existing Timeslice), through a single object.
 */
struct MicrosliceView {
    MicrosliceView(MicrosliceDescriptor d, const uint8_t *content);

    const MicrosliceDescriptor& desc() const { return _desc; };
    const uint8_t *content() const { return _content; };

private:
    MicrosliceDescriptor _desc; // TODO maybe store only pointer or reference
    const uint8_t *_content;
};

}
