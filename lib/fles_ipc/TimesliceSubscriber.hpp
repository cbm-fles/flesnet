// Copyright 2014 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceSubscriber class.
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
/**
 * \brief The TimesliceSubscriber class receives serialized timeslice data sets
 * from a zeromq socket.
 */
class TimesliceSubscriber : public TimesliceSource
{
public:
    /// Construct timeslice subscriber receiving from given ZMQ address.
    TimesliceSubscriber(const std::string& address);

    /// Delete copy constructor (non-copyable).
    TimesliceSubscriber(const TimesliceSubscriber&) = delete;
    /// Delete assignment operator (non-copyable).
    void operator=(const TimesliceSubscriber&) = delete;

    virtual ~TimesliceSubscriber(){};

    /**
     * \brief Retrieve the next item.
     *
     * This function blocks if the next item is not yet available.
     *
     * \return pointer to the item, or nullptr if end-of-file
     */
    std::unique_ptr<StorableTimeslice> get()
    {
        return std::unique_ptr<StorableTimeslice>(do_get());
    };

private:
    virtual StorableTimeslice* do_get() override;

    zmq::context_t context_{1};
    zmq::socket_t subscriber_{context_, ZMQ_SUB};
};

} // namespace fles {
