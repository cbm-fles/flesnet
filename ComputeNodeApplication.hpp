#pragma once
/**
 * \file ComputeNodeApplication.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

/// Compute application class.
/** The ComputeNodeApplication object represents an instance of the
    running compute node application. */

class ComputeNodeApplication : public Application<ComputeBuffer>
{
public:
    /// The ComputeNodeApplication contructor.
    ComputeNodeApplication(Parameters& par, std::vector<unsigned> indexes) :
        Application<ComputeBuffer>(par)
    {
        //set_cpu(1);

        for (unsigned i: indexes) {
            std::unique_ptr<ComputeBuffer> buffer(new ComputeBuffer(i));
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
        sigaction(SIGCHLD, &sa, NULL);
    };

    ComputeNodeApplication(const ComputeNodeApplication&) = delete;
    void operator=(const ComputeNodeApplication&) = delete;

private:
    static void child_handler(int sig) {
        pid_t pid;
        int status;

        while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            /* Process with PID 'pid' has exited, handle it */
            int idx = -1;
            for (std::size_t i = 0; i != child_pids.size(); ++i)
                if (child_pids[i] == pid) idx = i;
            if (idx < 0) {
                //out.error() << "unknown child process died";
            } else {
                std::cerr << "child process " << idx << " died";
                ComputeBuffer::start_processor_task(idx);
            }
        }
    };
};
