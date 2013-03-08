/**
 * \file Application.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <iostream>
#include <chrono>
#include <boost/thread.hpp>
#include "InputBuffer.hpp"
#include "ComputeBuffer.hpp"
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
                               (par->base_port() + i));
    
        ib.connect(_par.compute_nodes(), services);
        ib.handle_cm_events(true);
        boost::thread t1(&InputBuffer::completion_handler, &ib);
        auto time1 = std::chrono::high_resolution_clock::now();
        ib.sender_loop();
        auto time2 = std::chrono::high_resolution_clock::now();
        auto runtime = std::chrono::duration_cast<std::chrono::microseconds>(time2 - time1).count();
        t1.join();
        boost::thread t2(&InputBuffer::handle_cm_events, &ib, false);
        ib.disconnect();
        t2.join();

        out.info() << ib.aggregate_content_bytes_sent() << " content bytes";
        out.info() << ib.aggregate_send_requests() << " SEND requests";
        out.info() << ib.aggregate_recv_requests() << " RECV requests";
        double rate = (double) ib.aggregate_bytes_sent() / (double) runtime;
        out.info() << "summary: " << ib.aggregate_bytes_sent()
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
        cb->accept(par->base_port() + par->node_index());
        cb->handle_cm_events(true);       
        boost::thread t1(&ComputeBuffer::handle_cm_events, cb, false);
        cb->completion_handler();
        t1.join();
        delete cb;
        
        return 0;
    }
};


#endif /* APPLICATION_HPP */
