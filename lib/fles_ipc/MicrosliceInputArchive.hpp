// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ArchiveDescriptor.hpp"
#include "InputArchive.hpp"

namespace fles
{

class Microslice;
class StorableMicroslice;

//! The MicrosliceInputArchive deserializes microslice data sets from an input
// file.
typedef InputArchive<Microslice, StorableMicroslice,
                     ArchiveType::MicrosliceArchive> MicrosliceInputArchive;

} // namespace fles {
