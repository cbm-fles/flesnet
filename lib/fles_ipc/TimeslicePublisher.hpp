// Copyright 2014 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimeslicePublisher class.
#pragma once

#include "StorableTimeslice.hpp"
#include "Sink.hpp"
#include <zmq.hpp>
#include <string>

namespace fles
{

/**
 * \brief The TimeslicePublisher class publishes serialized timeslice data sets
 * to a zeromq socket.
 */
class TimeslicePublisher : public TimesliceSink
{
public:
    /// Construct timeslice publisher sending at given ZMQ address.
    TimeslicePublisher(const std::string& address);

    /// Delete copy constructor (non-copyable).
    TimeslicePublisher(const TimeslicePublisher&) = delete;
    /// Delete assignment operator (non-copyable).
    void operator=(const TimeslicePublisher&) = delete;

    /// Send a timeslice to all connected subscribers.
    virtual void put(const fles::Timeslice& timeslice) override
    {
        do_put(timeslice);
    };

    /// Deprecated alternative to put().
    void publish(const fles::Timeslice& timeslice) { put(timeslice); };

private:
    zmq::context_t context_{1};
    zmq::socket_t publisher_{context_, ZMQ_PUB};
    std::string serial_str_;

    void do_put(const fles::StorableTimeslice& timeslice);
};

} // namespace fles {
