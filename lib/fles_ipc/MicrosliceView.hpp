#pragma once

#include "MicrosliceDescriptor.hpp"
#include "Microslice.hpp"

namespace fles
{

/**
 * Provide read access to microslice metadata and content stored elsewhere
 * (for example in an already existing Timeslice), through a single object.
 */
class MicrosliceView : public Microslice
{
public:
    MicrosliceView(const MicrosliceDescriptor& d, const uint8_t* content);
};

} // namespace fles {
