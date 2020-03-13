// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "StorableTimeslice.hpp"

namespace fles {

StorableTimeslice::StorableTimeslice(const StorableTimeslice& ts)
    : Timeslice(ts), data_(ts.data_), desc_(ts.desc_) {
  init_pointers();
}

StorableTimeslice::StorableTimeslice(StorableTimeslice&& ts) noexcept
    : Timeslice(ts), data_(std::move(ts.data_)), desc_(std::move(ts.desc_)) {
  init_pointers();
}

StorableTimeslice::StorableTimeslice(const Timeslice& ts)
    : data_(ts.timeslice_descriptor_.num_components),
      desc_(ts.timeslice_descriptor_.num_components) {
  timeslice_descriptor_ = ts.timeslice_descriptor_;
  for (std::size_t component = 0;
       component < ts.timeslice_descriptor_.num_components; ++component) {
    uint64_t size = ts.desc_ptr_[component]->size;
    data_[component].resize(size);
    std::copy_n(ts.data_ptr_[component], size, data_[component].begin());
    desc_[component] = *ts.desc_ptr_[component];
  }

  init_pointers();
}

StorableTimeslice::StorableTimeslice() = default;

} // namespace fles
