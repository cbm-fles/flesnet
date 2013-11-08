// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "fles_ipc.hpp"

namespace fles
{

TimesliceView::~TimesliceView()
{
    try
    {
        _completions_mq->send(&_completion, sizeof(_completion), 0);
    }
    catch (boost::interprocess::interprocess_exception& e)
    {
        // TODO: this is not sufficient
        std::cerr << "exception in destructor ~TimesliceView(): " << e.what();
    }
}

TimesliceView::TimesliceView(
    TimesliceWorkItem work_item,
    uint8_t* data,
    TimesliceComponentDescriptor* desc,
    std::shared_ptr<boost::interprocess::message_queue> completions_mq)
    : _work_item(std::move(work_item)),
      _data(data),
      _desc(desc),
      _completions_mq(std::move(completions_mq))
{
    _completion = {_work_item.ts_pos};
    _descriptor_offset = _work_item.ts_pos
                         & ((1L << _work_item.desc_buffer_size_exp) - 1L);
    _data_offset_mask = (1L << _work_item.data_buffer_size_exp) - 1L;

    // consistency check
    for (size_t c = 1; c < num_components(); ++c)
        if (this->desc(0).ts_num != this->desc(c).ts_num)
            std::cerr << "error: ts_num[0]=" << this->desc(0).ts_num
                      << ", ts_num[" << c << "]=" << this->desc(c).ts_num
                      << std::endl;
}

StorableTimeslice::StorableTimeslice(const TimesliceView& ts)
    : _work_item(ts._work_item),
      _data(ts._work_item.num_components),
      _index(ts.index())
{
    for (std::size_t component = 0; component < ts._work_item.num_components;
         ++component) {
        uint64_t size = ts.desc(component).size;
        const uint8_t* begin = &ts.data(component, ts.desc(component).offset);
        _data[component].resize(size);
        std::copy_n(begin, size, _data[component].begin());
    }
}

time_t TimesliceArchiveDescriptor::time_created() const
{
    return _time_created;
}

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

std::unique_ptr<const TimesliceView> TimesliceReceiver::receive()
{
    if (_eof)
        return nullptr;

    TimesliceWorkItem wi;
    std::size_t recvd_size;
    unsigned int priority;

    _work_items_mq->receive(&wi, sizeof(wi), recvd_size, priority);
    if (recvd_size == 0) {
        _eof = true;
        return nullptr;
    }
    assert(recvd_size == sizeof(wi));

    return std::unique_ptr<TimesliceView>(new TimesliceView(
        wi, reinterpret_cast<uint8_t*>(_data_region->get_address()),
        reinterpret_cast
        <TimesliceComponentDescriptor*>(_desc_region->get_address()),
        _completions_mq));
}

} // namespace fles {
