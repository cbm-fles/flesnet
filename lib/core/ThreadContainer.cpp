/**
 * \file ThreadContainer.cpp
 *
 * 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "ThreadContainer.hpp"
#include "global.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

void ThreadContainer::set_cpu(int n)
{
    int nprocs = sysconf(_SC_NPROCESSORS_CONF);
    if (n >= nprocs) {
        out.error() << "set_cpu: CPU " << n << " is not in range 0.."
                    << (nprocs - 1);
        return;
    }

    cpu_set_t cpu_mask;
    CPU_ZERO(&cpu_mask);
    CPU_SET(n, &cpu_mask);

    if (sched_setaffinity(0, sizeof(cpu_mask), &cpu_mask) != 0) {
        out.error() << "set_cpu: could not set CPU affinity";
    }
}

#pragma GCC diagnostic pop
