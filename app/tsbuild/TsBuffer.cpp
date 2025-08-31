// Copyright 2016-2020 Jan de Cuveland <cmail@cuveland.de>

#include "TsBuffer.hpp"
#include "SubTimeslice.hpp"
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
      shm_identifier_(std::move(shm_identifier)), buffer_size_(buffer_size) {
  boost::uuids::random_generator uuid_gen;
  shm_uuid_ = uuid_gen();

  boost::interprocess::shared_memory_object::remove(shm_identifier_.c_str());

  constexpr size_t overhead_size = 4096; // Wild guess, let's hope it's enough
  size_t managed_shm_size = buffer_size + overhead_size;

  INFO("Creating shared memory segment '{}' of size {} MiB", shm_identifier_,
       managed_shm_size / static_cast<long>(1024 * 1024));
  managed_shm_ = std::make_unique<boost::interprocess::managed_shared_memory>(
      boost::interprocess::create_only, shm_identifier_.c_str(),
      managed_shm_size);

  managed_shm_->construct<boost::uuids::uuid>(
      boost::interprocess::unique_instance)(shm_uuid_);
  DEBUG("Shared memory segment '{}' initialized", shm_identifier_);
}

TsBuffer::~TsBuffer() {
  INFO("Removing shared memory segment '{}'", shm_identifier_);
  boost::interprocess::shared_memory_object::remove(shm_identifier_.c_str());
}

void TsBuffer::send_work_item(std::byte* buffer,
                              TsID id,
                              const StDescriptor& ts_desc) {
  TsDescriptorShm item;
  item.shm_uuid = shm_uuid_;
  item.shm_identifier = shm_identifier_;
  item.offset = managed_shm_->get_handle_from_address(buffer);
  item.ts_desc = ts_desc;

  std::vector<std::byte> bytes = to_bytes(item);
  std::string bytes_str(reinterpret_cast<const char*>(bytes.data()),
                        bytes.size());
  ItemProducer::send_work_item(id, bytes_str);
  outstanding_.insert(id);
}
