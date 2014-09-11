// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "StorableTimeslice.hpp"

namespace fles
{

StorableTimeslice::StorableTimeslice(const StorableTimeslice& ts)
    : Timeslice(ts),
      _data(ts._data),
      _desc(ts._desc)
{
    init_pointers();
}

StorableTimeslice::StorableTimeslice(StorableTimeslice&& ts)
    : Timeslice(std::move(ts)),
      _data(std::move(ts._data)),
      _desc(std::move(ts._desc))
{
    init_pointers();
}

StorableTimeslice::StorableTimeslice(const Timeslice& ts)
    : _data(ts._timeslice_descriptor.num_components),
      _desc(ts._timeslice_descriptor.num_components)
{
    _timeslice_descriptor = ts._timeslice_descriptor;
    for (std::size_t component = 0;
         component < ts._timeslice_descriptor.num_components; ++component) {
        uint64_t size = ts._desc_ptr[component]->size;
        _data[component].resize(size);
        std::copy_n(ts._data_ptr[component], size, _data[component].begin());
        _desc[component] = *ts._desc_ptr[component];
    }

    init_pointers();
}

StorableTimeslice::StorableTimeslice() {}

} // namespace fles {
