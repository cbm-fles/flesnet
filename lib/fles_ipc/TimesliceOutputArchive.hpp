// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceOutputArchive class type.
#pragma once

#include "OutputArchive.hpp"
#include "StorableTimeslice.hpp"

namespace fles
{

/**
 * \brief The TimesliceOutputArchive class serializes timeslice data sets to
 * an output file.
 */
using TimesliceOutputArchive =
    OutputArchive<StorableTimeslice, ArchiveType::TimesliceArchive>;

} // namespace fles {
