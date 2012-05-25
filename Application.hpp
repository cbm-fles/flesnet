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

        std::vector<std::string> services;
        for (unsigned int i = 0; i < _par.computeNodes().size(); i++)
            services.push_back(boost::lexical_cast<std::string>(Par->basePort() + i));
    
        ib.initiateConnect(_par.computeNodes(), services);
        ib.handleCmEvents();
        boost::thread t1(&InputBuffer::senderLoop, &ib);
        boost::thread t2(&InputBuffer::completionHandler, &ib);

        t1.join();
        t2.join();
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
};


#endif /* APPLICATION_HPP */
