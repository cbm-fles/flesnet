// Copyright 2015, 2020 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::MicrosliceInputArchive class type.
#pragma once

#include "ArchiveDescriptor.hpp"
#include "InputArchive.hpp"
#include "InputArchiveLoop.hpp"
#include "InputArchiveSequence.hpp"

namespace fles {

class Microslice;
class StorableMicroslice;

/**
 * \brief The MicrosliceInputArchive class deserializes microslice data sets
 * from an input file.
 */
using MicrosliceInputArchive = InputArchive<Microslice,
                                            StorableMicroslice,
                                            ArchiveType::MicrosliceArchive>;

using MicrosliceInputArchiveLoop =
    InputArchiveLoop<Microslice,
                     StorableMicroslice,
                     ArchiveType::MicrosliceArchive>;

using MicrosliceInputArchiveSequence =
    InputArchiveSequence<Microslice,
                         StorableMicroslice,
                         ArchiveType::MicrosliceArchive>;

} // namespace fles
