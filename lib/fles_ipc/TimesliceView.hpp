// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "MicrosliceDescriptor.hpp"
#include "TimesliceWorkItem.hpp"
#include "TimesliceCompletion.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include <cstdint>
#include <memory>
#include <boost/interprocess/ipc/message_queue.hpp>

//! \file
//! This file describes the timeslice-based interface to FLES.

namespace fles
{

//! The TimesliceView class provides access to the data of a single timeslice in
// memory.
class TimesliceView
{
public:
    TimesliceView(const TimesliceView&) = delete;
    void operator=(const TimesliceView&) = delete;

    ~TimesliceView();

    /// Retrieve the timeslice index.
    uint64_t index() const
    {
        return _desc_ptr[0]->ts_num;
    }

    /// Retrieve the number of core microslices.
    uint64_t num_core_microslices() const
    {
        return _work_item.ts_desc.num_core_microslices;
    }

    /// Retrieve the total number of microslices.
    uint64_t num_microslices(uint64_t component) const
    {
        return _desc_ptr[component]->num_microslices;
    }

    /// Retrieve the number of components (contributing input channels).
    uint64_t num_components() const
    {
        return _work_item.ts_desc.num_components;
    }

    /// Retrieve a pointer to the data content of a given microslice
    const uint8_t* content(uint64_t component, uint64_t microslice) const
    {
        return _data_ptr[component] + _desc_ptr[component]->num_microslices
                                      * sizeof(MicrosliceDescriptor)
               + descriptor(component, microslice).offset
               - descriptor(component, 0).offset;
    }

    /// Retrieve the descriptor of a given microslice
    const MicrosliceDescriptor& descriptor(uint64_t component,
                                           uint64_t microslice) const
    {
        return reinterpret_cast
            <const MicrosliceDescriptor*>(_data_ptr[component])[microslice];
    }

private:
    friend class TimesliceReceiver;
    friend class StorableTimeslice;

    TimesliceView(TimesliceWorkItem work_item,
                  uint8_t* data,
                  TimesliceComponentDescriptor* desc,
                  std::shared_ptr
                  <boost::interprocess::message_queue> completions_mq);

    const uint8_t& data(uint64_t component, uint64_t offset) const
    {
        return (_data + (component << _work_item.data_buffer_size_exp))
            [offset & _data_offset_mask];
    }

    const TimesliceComponentDescriptor& desc(uint64_t component) const
    {
        return (_desc + (component << _work_item.desc_buffer_size_exp))
            [_descriptor_offset];
    }

    TimesliceWorkItem _work_item;
    TimesliceCompletion _completion = TimesliceCompletion();

    const uint8_t* const _data;
    const TimesliceComponentDescriptor* const _desc;

    std::shared_ptr<boost::interprocess::message_queue> _completions_mq;

    uint64_t _descriptor_offset;
    uint64_t _data_offset_mask;

    std::vector<const uint8_t*> _data_ptr;
    std::vector<const TimesliceComponentDescriptor*> _desc_ptr;
};

} // namespace fles {
