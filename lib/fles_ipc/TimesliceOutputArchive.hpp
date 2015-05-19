// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "OutputArchive.hpp"
#include "StorableTimeslice.hpp"

namespace fles
{

using TimesliceOutputArchive =
    OutputArchive<StorableTimeslice, ArchiveType::TimesliceArchive>;

} // namespace fles {
