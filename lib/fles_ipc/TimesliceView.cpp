// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceView.hpp"
#include <boost/archive/binary_iarchive.hpp>
#include <iostream>
#include <sstream>

namespace fles {

TimesliceView::TimesliceView(
    std::shared_ptr<boost::interprocess::managed_shared_memory> managed_shm,
    std::shared_ptr<const Item> work_item)
    : managed_shm_(std::move(managed_shm)), work_item_(std::move(work_item)) {

  std::istringstream istream(work_item_->payload());
  {
    boost::archive::binary_iarchive iarchive(istream);
    iarchive >> timeslice_item_;
  }
  timeslice_descriptor_ = timeslice_item_.ts_desc;

  // initialize access pointer vectors
  data_ptr_.resize(num_components());
  desc_ptr_.resize(num_components());

  for (size_t c = 0; c < num_components(); ++c) {
    desc_ptr_[c] = reinterpret_cast<fles::TimesliceComponentDescriptor*>(
        managed_shm_->get_address_from_handle(timeslice_item_.desc[c]));
    data_ptr_[c] = static_cast<uint8_t*>(
        managed_shm_->get_address_from_handle(timeslice_item_.data[c]));
  }

  // consistency check
  for (size_t c = 1; c < num_components(); ++c) {
    if (timeslice_descriptor_.index != desc_ptr_[c]->ts_num) {
      std::cerr << "error: index=" << timeslice_descriptor_.index << ", ts_num["
                << c << "]=" << desc_ptr_[c]->ts_num << std::endl;
    }
  }
}

} // namespace fles
