// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ArchiveDescriptor.hpp"
#include "InputArchive.hpp"

namespace fles
{

class Timeslice;
class StorableTimeslice;

//! The TimesliceInputArchive deserializes timeslice data sets from an input
// file.
typedef InputArchive<Timeslice, StorableTimeslice,
                     ArchiveType::TimesliceArchive> TimesliceInputArchive;

} // namespace fles {
