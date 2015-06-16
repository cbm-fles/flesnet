// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ArchiveDescriptor.hpp"
#include "InputArchive.hpp"

namespace fles
{

class Microslice;
class StorableMicroslice;

/**
 * @brief The MicrosliceInputArchive class deserializes microslice data sets
 * from an input file.
 */
using MicrosliceInputArchive = InputArchive<Microslice, StorableMicroslice,
                                            ArchiveType::MicrosliceArchive>;

} // namespace fles {
