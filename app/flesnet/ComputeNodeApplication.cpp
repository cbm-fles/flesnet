/**
 * \file ComputeNodeApplication.cpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "ComputeNodeApplication.hpp"
#include <numa.h>

ComputeNodeApplication::ComputeNodeApplication(Parameters& par,
                                               std::vector<unsigned> indexes)
    : Application<ComputeBuffer>(par)
{
    // set_cpu(1);

    for (unsigned i : indexes) {
        std::unique_ptr<ComputeBuffer> buffer(
            new ComputeBuffer(i,
                              _par.cn_data_buffer_size_exp(),
                              _par.cn_desc_buffer_size_exp(),
                              _par.base_port() + _par.compute_indexes().at(0),
                              _par.input_nodes().size(),
                              _par.timeslice_size(),
                              _par.overlap_size(),
                              _par.processor_instances(),
                              _par.processor_executable()));
        _buffers.push_back(std::move(buffer));
    }

    assert(numa_available() != -1);

    struct bitmask* nodemask = numa_allocate_nodemask();
    numa_bitmask_setbit(nodemask, 0);
    numa_bitmask_setbit(nodemask, 1);
    numa_bind(nodemask);
    numa_free_nodemask(nodemask);
}
