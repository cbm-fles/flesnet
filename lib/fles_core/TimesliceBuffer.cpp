// Copyright 2016-2020 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceBuffer.hpp"
#include "ItemProducer.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include "TimesliceDescriptor.hpp"
#include "TimesliceShmWorkItem.hpp"
#include "TimesliceWorkItem.hpp"
#include "Utility.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/detail/segment_manager_helper.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

namespace zmq {
class context_t;
}

TimesliceBuffer::TimesliceBuffer(zmq::context_t& context,
                                 const std::string& distributor_address,
                                 std::string shm_identifier,
                                 uint32_t data_buffer_size_exp,
                                 uint32_t desc_buffer_size_exp,
                                 uint32_t num_input_nodes)
    : ItemProducer(context, distributor_address),
      shm_identifier_(std::move(shm_identifier)),
      data_buffer_size_exp_(data_buffer_size_exp),
      desc_buffer_size_exp_(desc_buffer_size_exp),
      num_input_nodes_(num_input_nodes) {
  boost::uuids::random_generator uuid_gen;
  shm_uuid_ = uuid_gen();
        std::cout << "shm_uuid: " << shm_uuid_ << std::endl;
  boost::interprocess::shared_memory_object::remove(shm_identifier_.c_str());

  std::size_t data_size =
      (UINT64_C(1) << data_buffer_size_exp_) * num_input_nodes_;
  assert(data_size != 0);

  std::size_t desc_buffer_size = (UINT64_C(1) << desc_buffer_size_exp_);
  std::size_t desc_size = desc_buffer_size * num_input_nodes_ *
                          sizeof(fles::TimesliceComponentDescriptor);
  assert(desc_size != 0);

  constexpr size_t overhead_size = 4096; // Wild guess, let's hope it's enough
  size_t managed_shm_size = data_size + desc_size + overhead_size;
  // size_t managed_shm_size = data_size + desc_size /*+ overhead_size*/;
  std::cout << "managed_shm_size: " << managed_shm_size << std::endl;
  // size_t managed_shm_size = 1024 * 8;

  managed_shm_ = std::make_unique<boost::interprocess::managed_shared_memory>(
      boost::interprocess::create_only, shm_identifier_.c_str(),
      managed_shm_size);

  managed_shm_->construct<boost::uuids::uuid>(
      boost::interprocess::unique_instance)(shm_uuid_);

  data_ptr_ = static_cast<uint8_t*>(managed_shm_->allocate(data_size));
  desc_ptr_ = reinterpret_cast<fles::TimesliceComponentDescriptor*>(
  managed_shm_->allocate(desc_size));
  // std::cout << reinterpret_cast<uint64_t>(managed_shm_->get_address()) << std::endl;
  // std::cout << reinterpret_cast<uint64_t>(data_ptr_) - reinterpret_cast<uint64_t>(managed_shm_->get_address())<< std::endl;
  // std::cout << managed_shm_->get_handle_from_address(data_ptr_) << std::endl;
  // std::cout << reinterpret_cast<uint64_t>(desc_ptr_) - reinterpret_cast<uint64_t>(managed_shm_->get_address())<< std::endl;
}

TimesliceBuffer::~TimesliceBuffer() {
  boost::interprocess::shared_memory_object::remove(shm_identifier_.c_str());
}

void TimesliceBuffer::send_work_item(fles::TimesliceWorkItem wi) {
  // Create and fill new TimesliceShmWorkItem to be sent via zmq
  fles::TimesliceShmWorkItem item;
  item.shm_uuid = shm_uuid_;
  item.shm_identifier = shm_identifier_;
  item.ts_desc = wi.ts_desc;
  const auto num_components = item.ts_desc.num_components;
  const auto ts_pos = item.ts_desc.ts_pos;
  item.data.resize(num_components);
  item.desc.resize(num_components);
  for (uint32_t c = 0; c < num_components; ++c) {
    fles::TimesliceComponentDescriptor* tsc_desc = &get_desc(c, ts_pos);
    uint8_t* tsc_data = &get_data(c, tsc_desc->offset);
    item.data[c] = managed_shm_->get_handle_from_address(tsc_data);
    item.desc[c] = managed_shm_->get_handle_from_address(tsc_desc);
  }

  std::ostringstream ostream;
  {
    boost::archive::binary_oarchive oarchive(ostream);
    oarchive << item;
  }
  outstanding_.insert(ts_pos);
  ItemProducer::send_work_item(ts_pos, ostream.str());
}

std::string TimesliceBuffer::description() const {
  size_t data_buffer_size = (UINT64_C(1) << data_buffer_size_exp_);
  size_t desc_buffer_size = (UINT64_C(1) << desc_buffer_size_exp_) *
                            sizeof(fles::TimesliceComponentDescriptor);
  size_t overall_size =
      num_input_nodes_ * (data_buffer_size + desc_buffer_size);

  std::string desc = shm_identifier_ + " {" +
                     boost::uuids::to_string(shm_uuid_) +
                     "}, size: " + std::to_string(num_input_nodes_) + " * (" +
                     human_readable_count(data_buffer_size) + " + " +
                     human_readable_count(desc_buffer_size) +
                     ") = " + human_readable_count(overall_size);
  return desc;
}

void* TimesliceBuffer::get_shm_ptr() {
  return managed_shm_->get_address();
}

uint64_t TimesliceBuffer::get_shm_size() {
  return managed_shm_->get_size();
}