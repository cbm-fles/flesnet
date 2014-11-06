// Copyright 2014 Jan de Cuveland <cmail@cuveland.de>

#include "TimeslicePublisher.hpp"
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <algorithm>

namespace fles
{

TimeslicePublisher::TimeslicePublisher(const std::string& address)
{
    _publisher.bind(address.c_str());
}

void TimeslicePublisher::publish(const StorableTimeslice& timeslice)
{
    // serialize timeslice to string
    _serial_str.clear();
    boost::iostreams::back_insert_device<std::string> inserter(_serial_str);
    boost::iostreams::stream<boost::iostreams::back_insert_device<std::string>>
        s(inserter);
    boost::archive::binary_oarchive oa(s);
    oa << timeslice;
    s.flush();

    zmq::message_t message(_serial_str.size());
    std::copy_n(static_cast<const char*>(_serial_str.data()), message.size(),
                static_cast<char*>(message.data()));
    _publisher.send(message);
}

} // namespace fles {
