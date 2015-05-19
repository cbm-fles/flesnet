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
using MicrosliceInputArchive = InputArchive<Microslice, StorableMicroslice,
                                            ArchiveType::MicrosliceArchive>;

} // namespace fles {
