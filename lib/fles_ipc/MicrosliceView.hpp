// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "MicrosliceDescriptor.hpp"
#include "Microslice.hpp"

namespace fles
{

/**
 * @brief The MicrosliceView class provides read access to a microslice stored
 * elsewhere.
 *
 * The data of the microslice (metadata and content) is not stored in the class,
 * but elsewhere (e.g., in an already existing Timeslice object).
 */
class MicrosliceView : public Microslice
{
public:
    MicrosliceView(const MicrosliceDescriptor& d, const uint8_t* content);
};

} // namespace fles {
