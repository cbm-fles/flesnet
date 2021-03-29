// Copyright 2014 Jan de Cuveland <cmail@cuveland.de>

#include "TimeslicePublisher.hpp"
#include <algorithm>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>

namespace fles {

TimeslicePublisher::TimeslicePublisher(const std::string& address,
                                       uint32_t hwm) {
  publisher_.set(zmq::sockopt::sndhwm, int(hwm));
  publisher_.bind(address.c_str());
}

void TimeslicePublisher::do_put(const StorableTimeslice& timeslice) {
  // serialize timeslice to string
  serial_str_.clear();
  boost::iostreams::back_insert_device<std::string> inserter(serial_str_);
  boost::iostreams::stream<boost::iostreams::back_insert_device<std::string>> s(
      inserter);
  boost::archive::binary_oarchive oa(s);
  oa << timeslice;
  s.flush();

  zmq::message_t message(serial_str_.size());
  std::copy_n(static_cast<const char*>(serial_str_.data()), message.size(),
              static_cast<char*>(message.data()));
  publisher_.send(message, zmq::send_flags::none);
}

} // namespace fles
