// Copyright 2021 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceAutoSource class.
#pragma once

#include "AutoSource.hpp"
#include "StorableTimeslice.hpp"
#include "TimesliceView.hpp"

namespace fles {

using TimesliceAutoSource = AutoSource<Timeslice,
                                       StorableTimeslice,
                                       TimesliceView,
                                       ArchiveType::TimesliceArchive>;

} // namespace fles
