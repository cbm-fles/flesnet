// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "OutputArchive.hpp"
#include "StorableMicroslice.hpp"

namespace fles
{

typedef OutputArchive<StorableMicroslice, ArchiveType::MicrosliceArchive>
    MicrosliceOutputArchive;

} // namespace fles {
