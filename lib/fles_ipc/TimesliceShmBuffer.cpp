/* Copyright (C) 2016-2026 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */

#include "TimesliceShmBuffer.hpp"
#include "SubTimeslice.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include "TimesliceDescriptor.hpp"
#include "TimesliceShmWorkItem.hpp"
#include "Utility.hpp"
#include "log.hpp"
#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/uuid/random_generator.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fles {

namespace {

std::unique_ptr<boost::interprocess::managed_shared_memory>
create_managed_shm(const std::string& identifier, std::size_t buffer_size) {
  boost::interprocess::shared_memory_object::remove(identifier.c_str());

  constexpr std::size_t overhead_size =
      4096; // Wild guess, let's hope it's enough
  std::size_t managed_shm_size = buffer_size + overhead_size;

  INFO("Creating shared memory segment '{}' of size {}", identifier,
       human_readable_count(managed_shm_size, true));
  return std::make_unique<boost::interprocess::managed_shared_memory>(
      boost::interprocess::create_only, identifier.c_str(), managed_shm_size);
}

} // namespace

TimesliceShmBuffer::TimesliceShmBuffer(zmq::context_t& context,
                                       std::string shm_identifier,
                                       std::size_t buffer_size)
    : m_shm_identifier(std::move(shm_identifier)), m_buffer_size(buffer_size),
      m_item_distributor(context,
                         "inproc://" + m_shm_identifier,
                         "ipc://@" + m_shm_identifier),
      m_managed_shm(create_managed_shm(m_shm_identifier, m_buffer_size)),
      m_producer(context, "inproc://" + m_shm_identifier),
      m_distributor_thread(std::ref(m_item_distributor)) {
  boost::uuids::random_generator uuid_gen;
  m_shm_uuid = uuid_gen();
  m_managed_shm->construct<boost::uuids::uuid>(
      boost::interprocess::unique_instance)(m_shm_uuid);
  DEBUG("Shared memory segment '{}' initialized", m_shm_identifier);
}

TimesliceShmBuffer::~TimesliceShmBuffer() {
  m_item_distributor.stop();
  m_distributor_thread.join();
  INFO("Removing shared memory segment '{}'", m_shm_identifier);
  boost::interprocess::shared_memory_object::remove(m_shm_identifier.c_str());
}

void TimesliceShmBuffer::send_work_item(std::byte* buffer,
                                        tsb::TsId id,
                                        const tsb::StDescriptor& ts_desc) {
  TimesliceDescriptor d{};
  d.index = static_cast<uint64_t>(id);
  d.start_time = ts_desc.start_time_ns;
  d.duration = ts_desc.duration_ns;
  d.flags = ts_desc.flags;
  d.num_components = ts_desc.components.size();

  TimesliceShmWorkItem item{};
  item.shm_uuid = m_shm_uuid;
  item.shm_identifier = m_shm_identifier;
  item.ts_desc = d;
  for (const auto& c : ts_desc.components) {
    TimesliceComponentDescriptor tscd{};
    tscd.ts_num = static_cast<uint64_t>(id);
    tscd.offset = 0; // unused
    tscd.size = c.ms_data_size;
    tscd.num_microslices = c.num_microslices;
    tscd.flags = c.flags;
    item.data.push_back(
        m_managed_shm->get_handle_from_address(buffer + c.ms_data_offset));
    item.tsc_desc.push_back(tscd);
  }

  std::vector<std::byte> bytes = tsb::to_bytes(item);
  std::string bytes_str(reinterpret_cast<const char*>(bytes.data()),
                        bytes.size());
  m_producer.send_work_item(id, bytes_str);
  m_outstanding.insert(id);
}

std::optional<ItemID> TimesliceShmBuffer::try_receive_completion() {
  ItemID id{};
  if (!m_producer.try_receive_completion(&id)) {
    return std::nullopt;
  }
  if (m_outstanding.erase(id) != 1) {
    ERROR("Invalid item with id {}", id);
  }
  return id;
}

} // namespace fles
