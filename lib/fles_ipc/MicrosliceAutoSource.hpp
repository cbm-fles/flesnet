// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::MicrosliceAutoSource class.
#pragma once

#include "AutoSource.hpp"
#include "MicrosliceView.hpp"
#include "StorableMicroslice.hpp"

namespace fles {

/**
 * \brief A microslice source initialized from a locator string.
 *
 * The `file` and `tcp` schemes of AutoSource are available for microslices,
 * the `shm` scheme is not.
 */
using MicrosliceAutoSource = AutoSource<Microslice,
                                        StorableMicroslice,
                                        MicrosliceView,
                                        ArchiveType::MicrosliceArchive>;

} // namespace fles
