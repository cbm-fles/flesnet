#pragma once

#include "MicrosliceDescriptor.hpp"

namespace fles
{

/**
 * Provide read access to microslice metadata and content stored elsewhere
 * (for example in an already existing Timeslice), through a single object.
 */
class MicrosliceView : public Microslice
{
public:
    MicrosliceView(const MicrosliceDescriptor& d, const uint8_t *content);

    MicrosliceView(const MicrosliceView&) = delete;
    void operator=(const MicrosliceView&) = delete;
};

} // namespace fles {
