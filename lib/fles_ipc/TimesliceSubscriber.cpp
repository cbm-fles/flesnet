// Copyright 2014 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceSubscriber.hpp"

namespace fles
{

TimesliceSubscriber::TimesliceSubscriber(const std::string& address)
{
    _subscriber.connect(address.c_str());
    _subscriber.setsockopt(ZMQ_SUBSCRIBE, nullptr, 0);
}

fles::StorableTimeslice* TimesliceSubscriber::do_get()
{
    zmq::message_t message;
    _subscriber.recv(&message);

    boost::iostreams::basic_array_source<char> device(
        static_cast<char*>(message.data()), message.size());
    boost::iostreams::stream<boost::iostreams::basic_array_source<char>> s(
        device);
    boost::archive::binary_iarchive ia(s);

    fles::StorableTimeslice* sts = nullptr;
    try {
        sts = new fles::StorableTimeslice();
        ia >> *sts;
    } catch (boost::archive::archive_exception e) {
        delete sts;
        return nullptr;
    }
    return sts;
}

} // namespace fles {
