// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Timeslice.hpp"
#include "TimesliceWorkItem.hpp"
#include "TimesliceCompletion.hpp"
#include <cstdint>
#include <memory>
#include <boost/interprocess/ipc/message_queue.hpp>

//! \file
//! This file describes the timeslice-based interface to FLES.

namespace fles
{

//! The TimesliceView class provides access to the data of a single timeslice in
// memory.
class TimesliceView : public Timeslice
{
public:
    TimesliceView(const TimesliceView&) = delete;
    void operator=(const TimesliceView&) = delete;

    virtual ~TimesliceView();

private:
    friend class TimesliceReceiver;
    friend class StorableTimeslice;

    TimesliceView(
        TimesliceWorkItem work_item, uint8_t* data,
        TimesliceComponentDescriptor* desc,
        std::shared_ptr<boost::interprocess::message_queue> completions_mq);

    TimesliceCompletion _completion = TimesliceCompletion();

    std::shared_ptr<boost::interprocess::message_queue> _completions_mq;
};

} // namespace fles {
