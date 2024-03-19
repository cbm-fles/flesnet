// Copyright 2014 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimeslicePublisher class.
#pragma once

#include "Sink.hpp"
#include "StorableTimeslice.hpp"
#include <algorithm>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <string>
#include <zmq.hpp>

namespace fles {

/**
 * \brief The TimeslicePublisher class publishes serialized timeslice data sets
 * to a zeromq socket.
 */
template <class Base, class Derived> class Publisher : public Sink<Base> {
public:
  /// Construct timeslice publisher sending at given ZMQ address.
  explicit Publisher(const std::string& address, uint32_t hwm = 1) {
    publisher_.set(zmq::sockopt::sndhwm, int(hwm));
    publisher_.bind(address.c_str());
  }

  /// Delete copy constructor (non-copyable).
  Publisher(const Publisher&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const Publisher&) = delete;

  /// Send a timeslice to all connected subscribers.
  void put(std::shared_ptr<const Base> timeslice) override {
    do_put(*timeslice);
  };

private:
  zmq::context_t context_{1};
  zmq::socket_t publisher_{context_, ZMQ_PUB};
  std::string serial_str_;

  void do_put(const Derived& timeslice) {
    // serialize timeslice to string
    serial_str_.clear();
    boost::iostreams::back_insert_device<std::string> inserter(serial_str_);
    boost::iostreams::stream<boost::iostreams::back_insert_device<std::string>>
        s(inserter);
    boost::archive::binary_oarchive oa(s);
    oa << timeslice;
    s.flush();

    zmq::message_t message(serial_str_.size());
    std::copy_n(static_cast<const char*>(serial_str_.data()), message.size(),
                static_cast<char*>(message.data()));
    publisher_.send(message, zmq::send_flags::none);
  }
};

using TimeslicePublisher = Publisher<Timeslice, StorableTimeslice>;

} // namespace fles
