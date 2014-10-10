// Copyright 2014 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "TimesliceSource.hpp"
#include "StorableTimeslice.hpp"
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <zmq.hpp>
#include <string>

namespace fles
{

//! The TimesliceSubscriber receives serialized timeslice data sets from a
//zeromq socket.
class TimesliceSubscriber : public TimesliceSource
{
public:
    TimesliceSubscriber(const std::string& address);

    TimesliceSubscriber(const TimesliceSubscriber&) = delete;
    void operator=(const TimesliceSubscriber&) = delete;

    virtual ~TimesliceSubscriber(){};

    std::unique_ptr<StorableTimeslice> get()
    {
        return std::unique_ptr<StorableTimeslice>(do_get());
    };

private:
    virtual StorableTimeslice* do_get();

    zmq::context_t _context{1};
    zmq::socket_t _subscriber{_context, ZMQ_SUB};
};

} // namespace fles {
