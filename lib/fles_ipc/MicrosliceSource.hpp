// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Microslice.hpp"
#include "Source.hpp"

namespace fles
{

//! The MicrosliceSource base class implements the generic microslice-based
// input
// interface.
using MicrosliceSource = Source<Microslice>;

} // namespace fles {
