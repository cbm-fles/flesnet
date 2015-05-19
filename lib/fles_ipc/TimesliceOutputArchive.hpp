// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "OutputArchive.hpp"
#include "StorableTimeslice.hpp"

namespace fles
{

typedef OutputArchive<StorableTimeslice,
                      ArchiveDescriptor::ArchiveType::TimesliceArchive>
    TimesliceOutputArchive;

} // namespace fles {
