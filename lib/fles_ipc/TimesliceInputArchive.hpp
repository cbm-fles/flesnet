// Copyright 2015, 2020 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceInputArchive class type.
#pragma once

#include "ArchiveDescriptor.hpp"
#include "InputArchive.hpp"
#include "InputArchiveLoop.hpp"
#include "InputArchiveSequence.hpp"

namespace fles {

class Timeslice;
class StorableTimeslice;

/**
 * \brief The TimesliceInputArchive deserializes timeslice data sets from an
 * input file.
 */
using TimesliceInputArchive =
    InputArchive<Timeslice, StorableTimeslice, ArchiveType::TimesliceArchive>;

using TimesliceInputArchiveLoop =
    InputArchiveLoop<Timeslice,
                     StorableTimeslice,
                     ArchiveType::TimesliceArchive>;

using TimesliceInputArchiveSequence =
    InputArchiveSequence<Timeslice,
                         StorableTimeslice,
                         ArchiveType::TimesliceArchive>;

} // namespace fles
