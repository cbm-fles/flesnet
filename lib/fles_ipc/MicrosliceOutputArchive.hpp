// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::MicrosliceOutputArchive class type.
#pragma once

#include "OutputArchive.hpp"
#include "StorableMicroslice.hpp"

namespace fles
{

/**
 * \brief The MicrosliceOutputArchive class serializes microslice data sets to
 * an output file.
 */
using MicrosliceOutputArchive = OutputArchive<Microslice, StorableMicroslice,
                                              ArchiveType::MicrosliceArchive>;

} // namespace fles {
