/**
 * \file Application.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef APPLICATION_HPP
#define APPLICATION_HPP

/// %Application exception class.
/** An ApplicationException object signals a general error in the flow
    of the application. */

class ApplicationException : public std::runtime_error {
public:
    /// The ApplicationException default constructor.
    explicit ApplicationException(const std::string& what_arg = "")
        : std::runtime_error(what_arg) { }
};


/// %Application base class.
/** The Application object represents an instance of the running
    application. */

template<typename T>
class Application : public ThreadContainer
{
public:
    
    /// The Application contructor.
    explicit Application(Parameters const& par) : _par(par)
    { }

    void start()
    {
        for (auto& buffer: _buffers) {
            buffer->start();
        }
    }

    void join()
    {
        for (auto& buffer: _buffers) {
            buffer->join();
        }
    }
    
protected:

    /// The run parameters object.
    Parameters const& _par;

    /// The application's connection group / buffer objects
    std::vector<std::unique_ptr<T> > _buffers;
};


/// Input application class.
/** The InputApplication object represents an instance of the running
    input node application. */

class InputApplication : public Application<InputBuffer>
{
public:

    /// The InputApplication contructor.
    explicit InputApplication(Parameters& par, std::vector<unsigned> indexes) :
        Application<InputBuffer>(par),
        _compute_hostnames(par.compute_nodes())
    {
        for (unsigned int i = 0; i < par.compute_nodes().size(); i++)
            _compute_services.push_back(boost::lexical_cast<std::string>(par.base_port() + i));

        for (unsigned i: indexes) {
            std::unique_ptr<DataSource> data_source
                (new DummyFlib(par.in_data_buffer_size_exp(), par.in_desc_buffer_size_exp(),
                               i, par.check_pattern(), par.typical_content_size(),
                               par.randomize_sizes()));
            std::unique_ptr<InputBuffer> buffer
                (new InputBuffer(i, *data_source, _compute_hostnames, _compute_services,
                                 par.timeslice_size(), par.overlap_size(),
                                 par.max_timeslice_number()));
            _data_sources.push_back(std::move(data_source));
            _buffers.push_back(std::move(buffer));
        }
    }

private:
    std::vector<std::string> _compute_hostnames;
    std::vector<std::string> _compute_services;

    std::vector<std::unique_ptr<DataSource> > _data_sources;
};


/// Compute application class.
/** The ComputeApplication object represents an instance of the
    running compute node application. */

class ComputeApplication : public Application<ComputeBuffer>
{
public:

    /// The ComputeApplication contructor.
    explicit ComputeApplication(Parameters& par, std::vector<unsigned> indexes) :
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

private:

    static void child_handler(int sig) {
        pid_t pid;
        int status;

        while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            /* Process with PID 'pid' has exited, handle it */
            int idx = -1;
            for (std::size_t i = 0; i != child_pids.size(); i++)
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


#endif /* APPLICATION_HPP */
