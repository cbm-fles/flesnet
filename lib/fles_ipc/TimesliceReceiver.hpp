// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "TimesliceSource.hpp"
#include "TimesliceView.hpp"
#include <string>
#include <memory>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>

//! \file
//! This file describes the timeslice-based interface to FLES.

namespace fles
{

//! The TimesliceReveicer class implements the IPC mechanisms to receive a
// timeslice.
class TimesliceReceiver : public TimesliceSource
{
public:
    TimesliceReceiver(const std::string shared_memory_identifier);

    TimesliceReceiver(const TimesliceReceiver&) = delete;
    void operator=(const TimesliceReceiver&) = delete;

    virtual ~TimesliceReceiver() {};

    std::unique_ptr<TimesliceView> get()
    {
        return std::unique_ptr<TimesliceView>(do_get());
    };

private:
    virtual TimesliceView* do_get();

    const std::string _shared_memory_identifier;

    std::unique_ptr<boost::interprocess::shared_memory_object> _data_shm;
    std::unique_ptr<boost::interprocess::shared_memory_object> _desc_shm;

    std::unique_ptr<boost::interprocess::mapped_region> _data_region;
    std::unique_ptr<boost::interprocess::mapped_region> _desc_region;

    std::unique_ptr<boost::interprocess::message_queue> _work_items_mq;
    std::shared_ptr<boost::interprocess::message_queue> _completions_mq;
};

} // namespace fles {
