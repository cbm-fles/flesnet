// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "StorableTimeslice.hpp"

namespace fles
{

StorableTimeslice::StorableTimeslice(const TimesliceView& ts)
    : _work_item(ts._work_item),
      _data(ts._work_item.ts_desc.num_components),
      _desc(ts._work_item.ts_desc.num_components)
{
    for (std::size_t component = 0;
         component < ts._work_item.ts_desc.num_components;
         ++component) {
        uint64_t size = ts.desc(component).size;
        const uint8_t* begin = &ts.data(component, ts.desc(component).offset);
        _data[component].resize(size);
        std::copy_n(begin, size, _data[component].begin());
        _desc[component] = ts.desc(component);
    }

    init_pointers();
}

StorableTimeslice::StorableTimeslice()
{
}

} // namespace fles {
