// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceView.hpp"

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
    _completion = {_work_item.ts_desc.ts_pos};
    _descriptor_offset = _work_item.ts_desc.ts_pos
                         & ((1L << _work_item.desc_buffer_size_exp) - 1L);
    _data_offset_mask = (1L << _work_item.data_buffer_size_exp) - 1L;

    // initialize access pointer vectors
    _data_ptr.resize(num_components());
    _desc_ptr.resize(num_components());
    for (size_t c = 0; c < num_components(); ++c) {
        _desc_ptr[c] = &this->desc(c);
        _data_ptr[c] = &this->data(c, _desc_ptr[c]->offset);
    }

    // consistency check
    for (size_t c = 1; c < num_components(); ++c)
        if (_desc_ptr[0]->ts_num != _desc_ptr[c]->ts_num)
            std::cerr << "error: ts_num[0]=" << _desc_ptr[0]->ts_num
                      << ", ts_num[" << c << "]=" << _desc_ptr[c]->ts_num
                      << std::endl;
}

} // namespace fles {
