// Copyright 2016-2020 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceBufferFlex.hpp"
#include "TimesliceDescriptor.hpp"
#include "TimesliceShmWorkItem.hpp"
#include "TimesliceWorkItem.hpp"
#include "Utility.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cassert>
#include <memory>

namespace zmq {
class context_t;
}

TimesliceBufferFlex::TimesliceBufferFlex(zmq::context_t& context,
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

  managed_shm_ = std::make_unique<boost::interprocess::managed_shared_memory>(
      boost::interprocess::create_only, shm_identifier_.c_str(),
      managed_shm_size);

  managed_shm_->construct<boost::uuids::uuid>(
      boost::interprocess::unique_instance)(shm_uuid_);
}

TimesliceBufferFlex::~TimesliceBufferFlex() {
  boost::interprocess::shared_memory_object::remove(shm_identifier_.c_str());
}

void TimesliceBufferFlex::send_work_item(fles::TimesliceWorkItem wi) {
  // Create and fill new TimesliceShmWorkItem to be sent via zmq
  // TODO
  /*
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
  */
}
