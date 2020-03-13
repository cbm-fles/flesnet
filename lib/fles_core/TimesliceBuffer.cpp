// Copyright 2016 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceBuffer.hpp"

#include <utility>

TimesliceBuffer::TimesliceBuffer(std::string shm_identifier,
                                 uint32_t data_buffer_size_exp,
                                 uint32_t desc_buffer_size_exp,
                                 uint32_t num_input_nodes)
    : shm_identifier_(std::move(shm_identifier)),
      data_buffer_size_exp_(data_buffer_size_exp),
      desc_buffer_size_exp_(desc_buffer_size_exp),
      num_input_nodes_(num_input_nodes) {
  boost::interprocess::shared_memory_object::remove(
      (shm_identifier_ + "data_").c_str());
  boost::interprocess::shared_memory_object::remove(
      (shm_identifier_ + "desc_").c_str());

  std::unique_ptr<boost::interprocess::shared_memory_object> data_shm(
      new boost::interprocess::shared_memory_object(
          boost::interprocess::create_only, (shm_identifier_ + "data_").c_str(),
          boost::interprocess::read_write));
  data_shm_ = std::move(data_shm);

  std::unique_ptr<boost::interprocess::shared_memory_object> desc_shm(
      new boost::interprocess::shared_memory_object(
          boost::interprocess::create_only, (shm_identifier_ + "desc_").c_str(),
          boost::interprocess::read_write));
  desc_shm_ = std::move(desc_shm);

  std::size_t data_size =
      (UINT64_C(1) << data_buffer_size_exp_) * num_input_nodes_;
  assert(data_size != 0);
  data_shm_->truncate(static_cast<boost::interprocess::offset_t>(data_size));

  std::size_t desc_buffer_size = (UINT64_C(1) << desc_buffer_size_exp_);

  std::size_t desc_size = desc_buffer_size * num_input_nodes_ *
                          sizeof(fles::TimesliceComponentDescriptor);
  assert(desc_size != 0);
  desc_shm_->truncate(static_cast<boost::interprocess::offset_t>(desc_size));

  std::unique_ptr<boost::interprocess::mapped_region> data_region(
      new boost::interprocess::mapped_region(*data_shm_,
                                             boost::interprocess::read_write));
  data_region_ = std::move(data_region);

  std::unique_ptr<boost::interprocess::mapped_region> desc_region(
      new boost::interprocess::mapped_region(*desc_shm_,
                                             boost::interprocess::read_write));
  desc_region_ = std::move(desc_region);

// # TODO[jan]: with-valgrind optional in cmake
#if 0
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
        VALGRIND_MAKE_MEM_DEFINED(data_region_->get_address(),
                                  data_region_->get_size());
        VALGRIND_MAKE_MEM_DEFINED(desc_region_->get_address(),
                                  desc_region_->get_size());
#pragma GCC diagnostic pop
#endif

  boost::interprocess::message_queue::remove(
      (shm_identifier_ + "work_items_").c_str());
  boost::interprocess::message_queue::remove(
      (shm_identifier_ + "completions_").c_str());

  std::unique_ptr<boost::interprocess::message_queue> work_items_mq(
      new boost::interprocess::message_queue(
          boost::interprocess::create_only,
          (shm_identifier_ + "work_items_").c_str(), desc_buffer_size,
          sizeof(fles::TimesliceWorkItem)));
  work_items_mq_ = std::move(work_items_mq);

  std::unique_ptr<boost::interprocess::message_queue> completions_mq(
      new boost::interprocess::message_queue(
          boost::interprocess::create_only,
          (shm_identifier_ + "completions_").c_str(), desc_buffer_size,
          sizeof(fles::TimesliceCompletion)));
  completions_mq_ = std::move(completions_mq);
}

TimesliceBuffer::~TimesliceBuffer() {
  boost::interprocess::shared_memory_object::remove(
      (shm_identifier_ + "data_").c_str());
  boost::interprocess::shared_memory_object::remove(
      (shm_identifier_ + "desc_").c_str());
  boost::interprocess::message_queue::remove(
      (shm_identifier_ + "work_items_").c_str());
  boost::interprocess::message_queue::remove(
      (shm_identifier_ + "completions_").c_str());
}

uint8_t* TimesliceBuffer::get_data_ptr(uint_fast16_t index) {
  return static_cast<uint8_t*>(data_region_->get_address()) +
         index * (UINT64_C(1) << data_buffer_size_exp_);
}

fles::TimesliceComponentDescriptor*
TimesliceBuffer::get_desc_ptr(uint_fast16_t index) {
  return reinterpret_cast<fles::TimesliceComponentDescriptor*>(
             desc_region_->get_address()) +
         index * (UINT64_C(1) << desc_buffer_size_exp_);
}

uint8_t& TimesliceBuffer::get_data(uint_fast16_t index, uint64_t offset) {
  offset &= (UINT64_C(1) << data_buffer_size_exp_) - 1;
  return get_data_ptr(index)[offset];
}

fles::TimesliceComponentDescriptor&
TimesliceBuffer::get_desc(uint_fast16_t index, uint64_t offset) {
  offset &= (UINT64_C(1) << desc_buffer_size_exp_) - 1;
  return get_desc_ptr(index)[offset];
}
