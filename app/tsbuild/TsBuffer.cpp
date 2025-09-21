// Copyright 2016-2020 Jan de Cuveland <cmail@cuveland.de>

#include "TsBuffer.hpp"
#include "SubTimeslice.hpp"
#include "TimesliceDescriptor.hpp"
#include "TimesliceShmWorkItem.hpp"
#include "Utility.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cassert>
#include <memory>

namespace zmq {
class context_t;
}

TsBuffer::TsBuffer(zmq::context_t& context,
                   const std::string& distributor_address,
                   std::string shm_identifier,
                   std::size_t buffer_size)
    : ItemProducer(context, distributor_address),
      m_shm_identifier(std::move(shm_identifier)), m_buffer_size(buffer_size) {
  boost::uuids::random_generator uuid_gen;
  m_shm_uuid = uuid_gen();

  boost::interprocess::shared_memory_object::remove(m_shm_identifier.c_str());

  constexpr size_t overhead_size = 4096; // Wild guess, let's hope it's enough
  size_t managed_shm_size = m_buffer_size + overhead_size;

  INFO("Creating shared memory segment '{}' of size {}", m_shm_identifier,
       human_readable_count(managed_shm_size, true));
  m_managed_shm = std::make_unique<boost::interprocess::managed_shared_memory>(
      boost::interprocess::create_only, m_shm_identifier.c_str(),
      managed_shm_size);

  m_managed_shm->construct<boost::uuids::uuid>(
      boost::interprocess::unique_instance)(m_shm_uuid);
  DEBUG("Shared memory segment '{}' initialized", m_shm_identifier);
}

TsBuffer::~TsBuffer() {
  INFO("Removing shared memory segment '{}'", m_shm_identifier);
  boost::interprocess::shared_memory_object::remove(m_shm_identifier.c_str());
}

void TsBuffer::send_work_item(std::byte* buffer,
                              TsId id,
                              const StDescriptor& ts_desc) {
  fles::TimesliceDescriptor d{};
  d.index = static_cast<uint64_t>(id);
  d.start_time = ts_desc.start_time_ns;
  d.duration = ts_desc.duration_ns;
  d.flags = ts_desc.flags;
  d.num_components = ts_desc.components.size();

  fles::TimesliceShmWorkItem item{};
  item.shm_uuid = m_shm_uuid;
  item.shm_identifier = m_shm_identifier;
  item.ts_desc = d;
  for (const auto& c : ts_desc.components) {
    fles::TimesliceComponentDescriptor tscd{};
    tscd.ts_num = static_cast<uint64_t>(id); // unused
    tscd.offset = 0;                         // unused
    tscd.size = c.ms_data_size;
    tscd.num_microslices = c.num_microslices;
    tscd.flags = c.flags;
    item.data.push_back(
        m_managed_shm->get_handle_from_address(buffer + c.ms_data_offset));
    item.tsc_desc.push_back(tscd);
  }

  std::vector<std::byte> bytes = to_bytes(item);
  std::string bytes_str(reinterpret_cast<const char*>(bytes.data()),
                        bytes.size());
  ItemProducer::send_work_item(id, bytes_str);
  m_outstanding.insert(id);
}
