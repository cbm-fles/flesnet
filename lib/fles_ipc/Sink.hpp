// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::Sink template class and related class types.
#pragma once

namespace fles
{

/**
 * \brief The Sink class implements the generic item-based output interface.
 *
 * This class is an abstract base class for several classes using an item-based
 * output interface.
 */
template <class T> class Sink
{
public:
    /// Receive an item to sink.
    virtual void put(const T& item) = 0;

    virtual ~Sink(){};
};

class Microslice;
class Timeslice;

/**
 * \brief The MicrosliceSink base class implements the generic
 * microslice-based output interface.
 */
using MicrosliceSink = Sink<Microslice>;

/**
* \brief The TimesliceSink base class implements the generic
* timeslice-based output interface.
*/
using TimesliceSink = Sink<Timeslice>;

} // namespace fles {
