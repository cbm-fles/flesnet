// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "ThreadContainer.hpp"
#include "log.hpp"
#ifdef HAVE_NUMA
#include <numa.h>
#endif

void ThreadContainer::set_node() {
#ifdef HAVE_NUMA
  if (numa_available() == -1) {
    L_(error) << "numa_available() failed";
    return;
  }

  struct bitmask* nodemask = numa_allocate_nodemask();
  numa_bitmask_setbit(nodemask, 0);
  if (numa_max_node() > 0) {
    numa_bitmask_setbit(nodemask, 1);
  }
  numa_bind(nodemask);
  numa_free_nodemask(nodemask);
#else
  L_(debug) << "set_node: built without libnuma";
#endif
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

void ThreadContainer::set_cpu(int n) {
#ifdef HAVE_NUMA
  int nprocs = sysconf(_SC_NPROCESSORS_ONLN);
  if (n >= nprocs) {
    L_(debug) << "set_cpu: CPU " << n << " is not in range 0.." << (nprocs - 1);
    return;
  }

  cpu_set_t cpu_mask;
  CPU_ZERO(&cpu_mask);
  CPU_SET(n, &cpu_mask);

  if (sched_setaffinity(0, sizeof(cpu_mask), &cpu_mask) != 0) {
    L_(error) << "set_cpu: could not set CPU affinity";
  }
#else
  (void)n;
  L_(debug) << "set_cpu: built without libnuma";
#endif
}

#pragma GCC diagnostic pop
