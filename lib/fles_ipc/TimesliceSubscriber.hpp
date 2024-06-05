// Copyright 2014 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceSubscriber class.
#pragma once

#include "StorableTimeslice.hpp"
#include "Subscriber.hpp"
#include "Timeslice.hpp"

namespace fles {
/**
 * \brief The TimesliceSubscriber class receives serialized timeslice data sets
 * from a zeromq socket.
 */
using TimesliceSubscriber = Subscriber<Timeslice, StorableTimeslice>;

} // namespace fles
