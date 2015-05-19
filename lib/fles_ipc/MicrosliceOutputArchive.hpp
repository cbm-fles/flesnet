// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "OutputArchive.hpp"
#include "StorableMicroslice.hpp"

namespace fles
{

using MicrosliceOutputArchive =
    OutputArchive<StorableMicroslice, ArchiveType::MicrosliceArchive>;

} // namespace fles {
