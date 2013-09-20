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

class Application : public ThreadContainer
{
public:
    
    /// The Application contructor.
    explicit Application(Parameters const& par) : _par(par) { };

    /// The "main" function of an application.
    virtual void run() = 0;

    void start() {
        _thread = boost::thread(&Application::run, this);
    }

    void join() {
        _thread.join();
    }
    
protected:

    /// The run parameters object.
    Parameters const& _par;

    /// The application's primary thread object
    boost::thread _thread;
};


/// Input application class.
/** The InputApplication object represents an instance of the running
    input node application. */

class InputApplication : public Application
{
public:

    /// The InputApplication contructor.
    explicit InputApplication(Parameters& par) : Application(par) { };
    
    /// The "main" function of an input node application.
    virtual void run() {
        set_cpu(0);

        std::vector<std::string> services;
        for (unsigned int i = 0; i < _par.compute_nodes().size(); i++)
            services.push_back(boost::lexical_cast<std::string>
                               (_par.base_port() + i));

        InputBuffer ib(_par.input_indexes().at(0));

        ib.connect(_par.compute_nodes(), services);
        ib.handle_cm_events(_par.compute_nodes().size());
        boost::thread t1(&InputBuffer::completion_handler, &ib);

        auto time1 = std::chrono::high_resolution_clock::now();
        ib.sender_loop();
        auto time2 = std::chrono::high_resolution_clock::now();
        auto runtime = std::chrono::duration_cast<std::chrono::microseconds>(time2 - time1).count();

        t1.join();
        boost::thread t2(&InputBuffer::handle_cm_events, &ib, 0);
        ib.disconnect();
        t2.join();

        ib.summary(runtime);
    };
};


/// Compute application class.
/** The ComputeApplication object represents an instance of the
    running compute node application. */

class ComputeApplication : public Application
{
public:

    /// The ComputeApplication contructor.
    explicit ComputeApplication(Parameters& par) : Application(par) {
        //set_cpu(1);

        assert(numa_available() != -1);

        struct bitmask* nodemask = numa_allocate_nodemask();
        numa_bitmask_setbit(nodemask, 0);
        numa_bitmask_setbit(nodemask, 1);
        numa_bind(nodemask);
        numa_free_nodemask(nodemask);

        std::unique_ptr<ComputeBuffer> cb(new ComputeBuffer(_par.compute_indexes().at(0)));
        _cb = std::move(cb);

        /* Establish SIGCHLD handler. */
        struct sigaction sa;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sa.sa_handler = child_handler;
        sigaction(SIGCHLD, &sa, NULL);
    };

    /// The "main" function of a compute node application.
    virtual void run() {
        //set_cpu(0);

        boost::thread_group analysis_threads;
        if (_par.processor_executable().empty()) {
            for (uint_fast32_t i = 1; i <= _par.processor_instances(); i++) {
                analysis_threads.create_thread(TimesliceProcessor(*_cb, i));
            }
        } else {
            child_pids.resize(_par.processor_instances());
            for (uint_fast32_t i = 1; i <= _par.processor_instances(); i++) {
                start_processor_task(i);
            }
        }
        boost::thread ts_compl(&ComputeBuffer::handle_ts_completion, _cb.get());

        _cb->accept(_par.base_port() + _par.compute_indexes().at(0), _par.input_nodes().size());
        _cb->handle_cm_events(_par.input_nodes().size());
        boost::thread t1(&ComputeBuffer::handle_cm_events, _cb.get(), 0);
        auto time1 = std::chrono::high_resolution_clock::now();
        _cb->completion_handler();
        auto time2 = std::chrono::high_resolution_clock::now();
        auto runtime = std::chrono::duration_cast<std::chrono::microseconds>(time2 - time1).count();
        if (_par.processor_executable().empty()) {
            analysis_threads.join_all();
        } else {
            for (uint_fast32_t i = 1; i <= _par.processor_instances(); i++) {
                pid_t pid = child_pids[i];
                child_pids[i] = 0;
                kill(pid, SIGTERM);
            }
        }
        ts_compl.join();
        t1.join();

        _cb->summary(runtime);
    }

private:
    std::unique_ptr<ComputeBuffer> _cb;

    static void start_processor_task(int i) {
        child_pids[i] = fork();
        if (!child_pids[i]) {
            std::stringstream s;
            s << i;
            execl(par->processor_executable().c_str(), s.str().c_str(), (char*) 0);
            out.fatal() << "Could not start processor task '" << par->processor_executable()
                        << " " << s.str() << "': " << strerror(errno);
            exit(1);
        }
    };

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
                start_processor_task(idx);
            }
        }
    };
};


#endif /* APPLICATION_HPP */
