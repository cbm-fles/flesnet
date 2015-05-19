// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Timeslice.hpp"
#include "Source.hpp"

namespace fles
{

//! The TimesliceSource base class implements the generic timeslice-based input
// interface.
using TimesliceSource = Source<Timeslice>;

} // namespace fles {
