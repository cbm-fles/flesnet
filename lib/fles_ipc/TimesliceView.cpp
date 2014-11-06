// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceView.hpp"

namespace fles
{

TimesliceView::TimesliceView(
    TimesliceWorkItem work_item, uint8_t* data,
    TimesliceComponentDescriptor* desc,
    std::shared_ptr<boost::interprocess::message_queue> completions_mq)
    : _completions_mq(std::move(completions_mq))
{
    _timeslice_descriptor = work_item.ts_desc;
    _completion = {_timeslice_descriptor.ts_pos};

    // initialize access pointer vectors
    _data_ptr.resize(num_components());
    _desc_ptr.resize(num_components());
    uint64_t descriptor_offset =
        _timeslice_descriptor.ts_pos &
        ((UINT64_C(1) << work_item.desc_buffer_size_exp) - 1);
    uint64_t data_offset_mask =
        (UINT64_C(1) << work_item.data_buffer_size_exp) - 1;
    for (size_t c = 0; c < num_components(); ++c) {
        _desc_ptr[c] =
            desc + (c << work_item.desc_buffer_size_exp) + descriptor_offset;
        _data_ptr[c] = data + (c << work_item.data_buffer_size_exp) +
                       (_desc_ptr[c]->offset & data_offset_mask);
    }

    // consistency check
    for (size_t c = 1; c < num_components(); ++c)
        if (_timeslice_descriptor.index != _desc_ptr[c]->ts_num)
            std::cerr << "error: index=" << _timeslice_descriptor.index
                      << ", ts_num[" << c << "]=" << _desc_ptr[c]->ts_num
                      << std::endl;
}

TimesliceView::~TimesliceView()
{
    try { _completions_mq->send(&_completion, sizeof(_completion), 0); }
    catch (boost::interprocess::interprocess_exception& e)
    {
        std::cerr << "exception in destructor ~TimesliceView(): " << e.what();
        // FIXME: this may not be sufficient in case of error
    }
}

} // namespace fles {
