/**
 * \file Application.hpp
 *
 * 2012, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <iostream>
#include <boost/cstdint.hpp>
#include <boost/thread.hpp>
#include "InputBuffer.hpp"
#include "ComputeBuffer.hpp"
#include "klepsydra.hpp"
#include "global.hpp"


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

class Application
{
public:
    
    /// The Application contructor.
    explicit Application(Parameters const& par) : _par(par) { };

    /// The "main" function of an application.
    virtual int run() = 0;
    
protected:

    /// The run parameters object.
    Parameters const& _par;
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
    virtual int run() {
        InputBuffer ib;

        // hacky delay to give CNs time to start
        boost::this_thread::sleep(boost::posix_time::millisec(500));

        std::vector<std::string> services;
        for (unsigned int i = 0; i < _par.compute_nodes().size(); i++)
            services.push_back(boost::lexical_cast<std::string>
                               (Par->base_port() + i));
    
        ib.connect(_par.compute_nodes(), services);
        ib.handle_cm_events(true);
        boost::thread t1(&InputBuffer::completion_handler, &ib);
        klepsydra::Monotonic timer;
        ib.sender_loop();
        uint64_t runtime = timer.getTime();
        t1.join();
        boost::thread t2(&InputBuffer::handle_cm_events, &ib, false);
        ib.disconnect();
        t2.join();

        Log.info() << ib.aggregate_content_bytes_sent() << " content bytes";
        Log.info() << ib.aggregate_send_requests() << " SEND requests";
        Log.info() << ib.aggregate_recv_requests() << " RECV requests";
        double rate = (double) ib.aggregate_bytes_sent() / (double) runtime;
        Log.info() << "summary: " << ib.aggregate_bytes_sent()
                   << " bytes sent in "
                   << runtime << " µs (" << rate << " MB/s)";
        
        return 0;
    };
};


/// Compute application class.
/** The ComputeApplication object represents an instance of the
    running compute node application. */

class ComputeApplication : public Application
{
public:

    /// The ComputeApplication contructor.
    explicit ComputeApplication(Parameters& par) : Application(par) { };

    /// The "main" function of a compute node application.
    virtual int run() {
        ComputeBuffer* cb = new ComputeBuffer();
        cb->accept(Par->base_port() + Par->node_index());
        cb->handle_cm_events(true);
        
        /// DEBUG v
        boost::thread t1(&ComputeBuffer::handle_cm_events, cb, false);
        /// DEBUG ^

        cb->completion_handler();
        t1.join();
        delete cb;
        
        return 0;
    }
};


#endif /* APPLICATION_HPP */
