// Copyright 2014 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "StorableTimeslice.hpp"
#include <zmq.hpp>
#include <string>

namespace fles
{

//! The TimeslicePublisher publishes serialized timeslice data sets to a zeromq
//socket.
class TimeslicePublisher
{
public:
    TimeslicePublisher(const std::string& address);

    TimeslicePublisher(const TimeslicePublisher&) = delete;
    void operator=(const TimeslicePublisher&) = delete;

    void publish(const fles::StorableTimeslice& timeslice);

private:
    zmq::context_t _context{1};
    zmq::socket_t _publisher{_context, ZMQ_PUB};
    std::string _serial_str;
};

} // namespace fles {
