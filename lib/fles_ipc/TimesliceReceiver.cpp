// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceReceiver.hpp"

namespace fles
{

TimesliceReceiver::TimesliceReceiver(const std::string shared_memory_identifier)
    : _shared_memory_identifier(shared_memory_identifier)
{
    _data_shm = std::unique_ptr<boost::interprocess::shared_memory_object>(
        new boost::interprocess::shared_memory_object(
            boost::interprocess::open_only,
            (shared_memory_identifier + "_data").c_str(),
            boost::interprocess::read_only));

    _desc_shm = std::unique_ptr<boost::interprocess::shared_memory_object>(
        new boost::interprocess::shared_memory_object(
            boost::interprocess::open_only,
            (shared_memory_identifier + "_desc").c_str(),
            boost::interprocess::read_only));

    _data_region = std::unique_ptr<boost::interprocess::mapped_region>(
        new boost::interprocess::mapped_region(*_data_shm,
                                               boost::interprocess::read_only));

    _desc_region = std::unique_ptr<boost::interprocess::mapped_region>(
        new boost::interprocess::mapped_region(*_desc_shm,
                                               boost::interprocess::read_only));

    _work_items_mq = std::unique_ptr<boost::interprocess::message_queue>(
        new boost::interprocess::message_queue(
            boost::interprocess::open_only,
            (shared_memory_identifier + "_work_items").c_str()));

    _completions_mq = std::shared_ptr<boost::interprocess::message_queue>(
        new boost::interprocess::message_queue(
            boost::interprocess::open_only,
            (shared_memory_identifier + "_completions").c_str()));
}

/*
 * This workaround is required for Boost versions 1.52.0 to 1.55.0, as there is
 * a serious synchronization bug in boost::interprocess in these versions.
 *
 * For reference, see: https://svn.boost.org/trac/boost/ticket/9221
 * ("message_queue deadlock on linux")
 *
 * This code should no longer be required when using Boost version 1.56.0 or
 * newer.
 *
 * 2014-03-12, Jan de Cuveland <cuveland@fias.uni-frankfurt.de>
 */
void mq_receive_workaround(boost::interprocess::message_queue& mq, void* buffer,
                           size_t buffer_size, size_t& recvd_size,
                           unsigned int& priority)
{
    boost::posix_time::ptime abs_time;
    do {
        abs_time = boost::posix_time::ptime(
                       boost::posix_time::microsec_clock::universal_time()) +
                   boost::posix_time::milliseconds(10);
    } while (
        !mq.timed_receive(buffer, buffer_size, recvd_size, priority, abs_time));
}

TimesliceView* TimesliceReceiver::do_get()
{
    if (_eof)
        return nullptr;

    TimesliceWorkItem wi;
    std::size_t recvd_size;
    unsigned int priority;

#if defined(BOOST_VERSION) and BOOST_VERSION >= 105600
    _work_items_mq->receive(&wi, sizeof(wi), recvd_size, priority);
#else
    mq_receive_workaround(*_work_items_mq, &wi, sizeof(wi), recvd_size,
                          priority);
#endif
    if (recvd_size == 0) {
        _eof = true;
        return nullptr;
    }
    assert(recvd_size == sizeof(wi));

    return new TimesliceView(
        wi, reinterpret_cast<uint8_t*>(_data_region->get_address()),
        reinterpret_cast<TimesliceComponentDescriptor*>(
            _desc_region->get_address()),
        _completions_mq);
}

} // namespace fles {
