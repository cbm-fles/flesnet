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
        for (unsigned int i = 0; i < _par.computeNodes().size(); i++)
            services.push_back(boost::lexical_cast<std::string>
                               (Par->basePort() + i));
    
        ib.connect(_par.computeNodes(), services);
        ib.handleCmEvents(true);
        boost::thread t1(&InputBuffer::completionHandler, &ib);
        klepsydra::Monotonic timer;
        ib.senderLoop();
        uint64_t runtime = timer.getTime();
        t1.join();
        boost::thread t2(&InputBuffer::handleCmEvents, &ib, false);
        ib.disconnect();
        t2.join();

        Log.info() << ib.aggregateContentBytesSent() << " content bytes";
        Log.info() << ib.aggregateSendRequests() << " SEND requests";
        Log.info() << ib.aggregateRecvRequests() << " RECV requests";
        double rate = (double) ib.aggregateBytesSent() / (double) runtime;
        Log.info() << "summary: " << ib.aggregateBytesSent()
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
    virtual int run();
//    virtual int run() {
//        ComputeBuffer cb;
// 
//        cb.connect(_par.inputNodes(), _par->basePort() + _par->nodeIndex());
//        cb.handleCmEvents(true);
//        boost::thread t1(&ComputeBuffer::completionHandler, &cb);
//        klepsydra::Monotonic timer;
//        //cb.dispatchLoop();
//        uint64_t runtime = timer.getTime();
//        t1.join();
//        boost::thread t2(&ComputeBuffer::handleCmEvents, &cb, false);
//        cb.disconnect();
//        t2.join();
// 
//        Log.info() << cb.aggregateContentBytesReceived() << " content bytes";
//        Log.info() << cb.aggregateSendRequests() << " SEND requests";
//        Log.info() << cb.aggregateRecvRequests() << " RECV requests";
//        double rate = (double) cb.aggregateBytesReceived() / (double) runtime;
//        Log.info() << "summary: " << cb.aggregateBytesReceived()
//                   << " bytes received in "
//                   << runtime << " µs (" << rate << " MB/s)";
//        
//        return 0;
//    }
};


#endif /* APPLICATION_HPP */
