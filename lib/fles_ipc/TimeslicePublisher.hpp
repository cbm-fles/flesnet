// Copyright 2014 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimeslicePublisher class.
#pragma once

#include "Publisher.hpp"
#include "StorableTimeslice.hpp"
#include "Timeslice.hpp"

namespace fles {
/**
 * \brief The TimeslicePublisher class publishes serialized timeslice data sets
 * to a zeromq socket.
 */
using TimeslicePublisher = Publisher<Timeslice, StorableTimeslice>;

} // namespace fles
