// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceReceiver.hpp"
#include <boost/version.hpp>
#include <memory>

namespace fles {

TimesliceReceiver::TimesliceReceiver(
    const std::string& shared_memory_identifier)
    : shared_memory_identifier_(shared_memory_identifier) {
  data_shm_ = std::unique_ptr<boost::interprocess::shared_memory_object>(
      new boost::interprocess::shared_memory_object(
          boost::interprocess::open_only,
          (shared_memory_identifier + "data_").c_str(),
          boost::interprocess::read_only));

  desc_shm_ = std::unique_ptr<boost::interprocess::shared_memory_object>(
      new boost::interprocess::shared_memory_object(
          boost::interprocess::open_only,
          (shared_memory_identifier + "desc_").c_str(),
          boost::interprocess::read_only));

  data_region_ = std::unique_ptr<boost::interprocess::mapped_region>(
      new boost::interprocess::mapped_region(*data_shm_,
                                             boost::interprocess::read_only));

  desc_region_ = std::unique_ptr<boost::interprocess::mapped_region>(
      new boost::interprocess::mapped_region(*desc_shm_,
                                             boost::interprocess::read_only));

  work_items_mq_ = std::unique_ptr<boost::interprocess::message_queue>(
      new boost::interprocess::message_queue(
          boost::interprocess::open_only,
          (shared_memory_identifier + "work_items_").c_str()));

  completions_mq_ = std::make_shared<boost::interprocess::message_queue>(
      boost::interprocess::open_only,
      (shared_memory_identifier + "completions_").c_str());
}

TimesliceView* TimesliceReceiver::do_get() {
  if (eos_) {
    return nullptr;
  }

  TimesliceWorkItem wi;
  std::size_t recvd_size;
  unsigned int priority;

  work_items_mq_->receive(&wi, sizeof(wi), recvd_size, priority);
  if (recvd_size == 0) {
    eos_ = true;
    // put end work item back for other consumers
    work_items_mq_->send(nullptr, 0, 0);
    return nullptr;
  }
  assert(recvd_size == sizeof(wi));

  return new TimesliceView(
      wi, reinterpret_cast<uint8_t*>(data_region_->get_address()),
      reinterpret_cast<TimesliceComponentDescriptor*>(
          desc_region_->get_address()),
      completions_mq_);
}

} // namespace fles
