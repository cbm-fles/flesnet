/**
 * \file ComputeNodeApplication.cpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "ComputeNodeApplication.hpp"
#include <csignal>
#include <numa.h>
#include <sys/wait.h>

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

    /* Establish SIGCHLD handler. */
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = child_handler;
    sigaction(SIGCHLD, &sa, nullptr);
}

void ComputeNodeApplication::child_handler(int sig)
{
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        /* Process with PID 'pid' has exited, handle it */
        int idx = -1;
        for (std::size_t i = 0; i != child_pids.size(); ++i)
            if (child_pids[i] == pid)
                idx = i;
        if (idx < 0) {
            // out.error() << "unknown child process died";
        } else {
            std::cerr << "child process " << idx << " died";
            ComputeBuffer::start_processor_task(idx, par->processor_executable());
        }
    }
}
