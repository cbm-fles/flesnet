// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceView.hpp"
#include <iostream>

namespace fles
{

TimesliceView::TimesliceView(
    TimesliceWorkItem work_item, uint8_t* data,
    TimesliceComponentDescriptor* desc,
    std::shared_ptr<boost::interprocess::message_queue> completions_mq)
    : completions_mq_(std::move(completions_mq))
{
    timeslice_descriptor_ = work_item.ts_desc;
    completion_ = {timeslice_descriptor_.ts_pos};

    // initialize access pointer vectors
    data_ptr_.resize(num_components());
    desc_ptr_.resize(num_components());
    uint64_t descriptor_offset =
        timeslice_descriptor_.ts_pos &
        ((UINT64_C(1) << work_item.desc_buffer_size_exp) - 1);
    uint64_t data_offset_mask =
        (UINT64_C(1) << work_item.data_buffer_size_exp) - 1;
    for (size_t c = 0; c < num_components(); ++c) {
        desc_ptr_[c] =
            desc + (c << work_item.desc_buffer_size_exp) + descriptor_offset;
        data_ptr_[c] = data + (c << work_item.data_buffer_size_exp) +
                       (desc_ptr_[c]->offset & data_offset_mask);
    }

    // consistency check
    for (size_t c = 1; c < num_components(); ++c)
        if (timeslice_descriptor_.index != desc_ptr_[c]->ts_num)
            std::cerr << "error: index=" << timeslice_descriptor_.index
                      << ", ts_num[" << c << "]=" << desc_ptr_[c]->ts_num
                      << std::endl;
}

TimesliceView::~TimesliceView()
{
    try {
        completions_mq_->send(&completion_, sizeof(completion_), 0);
    } catch (boost::interprocess::interprocess_exception& e) {
        std::cerr << "exception in destructor ~TimesliceView(): " << e.what();
        // FIXME: this may not be sufficient in case of error
    }
}

} // namespace fles {
