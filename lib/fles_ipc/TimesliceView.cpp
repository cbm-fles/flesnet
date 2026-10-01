// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceView.hpp"

#include "ItemWorkerProtocol.hpp"
#include "SubTimeslice.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include "TimesliceShmWorkItem.hpp"

#include <boost/interprocess/interprocess_fwd.hpp>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

namespace fles {

TimesliceView::TimesliceView(
    std::shared_ptr<boost::interprocess::managed_shared_memory> managed_shm,
    std::shared_ptr<const Item> work_item,
    const TimesliceShmWorkItem& timeslice_item)
    : managed_shm_(std::move(managed_shm)), work_item_(std::move(work_item)),
      timeslice_item_(timeslice_item) {

  timeslice_descriptor_ = timeslice_item.ts_desc;

  // initialize access pointer vectors
  data_ptr_.resize(num_components());
  desc_ptr_.resize(num_components());

  for (size_t c = 0; c < num_components(); ++c) {
    desc_ptr_.at(c) = &timeslice_item_.tsc_desc.at(c);
    data_ptr_.at(c) = static_cast<uint8_t*>(
        managed_shm_->get_address_from_handle(timeslice_item_.data.at(c)));
  }

  // consistency check
  for (size_t c = 1; c < num_components(); ++c) {
    if (timeslice_descriptor_.index != desc_ptr_.at(c)->ts_num) {
      std::cerr << "TimesliceView consistency check failed: index="
                << timeslice_descriptor_.index << ", ts_num[" << c
                << "]=" << desc_ptr_.at(c)->ts_num << std::endl;
    }
  }
}

tsb::StDescriptor TimesliceView::st_descriptor() const {
  const auto* base = static_cast<const uint8_t*>(managed_shm_->get_address());

  tsb::StDescriptor desc;
  desc.start_time_ns = start_time();
  desc.duration_ns = duration();
  desc.flags = flags();
  for (size_t c = 0; c < num_components(); ++c) {
    tsb::StComponentDescriptor& component = desc.components.emplace_back();
    component.ms_data_offset = data_ptr_[c] - base;
    component.ms_data_size = size_component(c);
    component.num_microslices = num_microslices(c);
    component.flags = desc_ptr_[c]->flags;
  }
  return desc;
}

} // namespace fles
