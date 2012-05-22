/*
 * \file Application.hpp
 *
 * 2012, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <iostream>
#include <boost/cstdint.hpp>
#include <boost/thread.hpp>
#include "Parameters.hpp"
#include "InputBuffer.hpp"


// timeslice component descriptor
/*
  typedef struct {
    uint64_t ts_num;
    uint64_t offset;
    uint64_t size;
} tscdesc_t;
*/

typedef struct {
    uint8_t hdrrev;
    uint8_t sysid;
    uint16_t flags;
    uint32_t size;
    uint64_t time;
} mc_hdr_t;


inline std::ostream& operator<<(std::ostream& s, REQUEST_ID v)
{
    switch (v) {
    case ID_WRITE_DATA:
        return s << "ID_WRITE_DATA";
    case ID_WRITE_DATA_WRAP:
        return s << "ID_WRITE_DATA_WRAP";
    case ID_WRITE_DESC:
        return s << "ID_WRITE_DESC";
    case ID_SEND_CN_WP:
        return s << "ID_SEND_CN_WP";
    case ID_RECEIVE_CN_ACK:
        return s << "ID_RECEIVE_CN_ACK";
    default:
        return s << (int) v;
    }
}


/// Application exception class.
/** An ApplicationException object signals a general error in the flow
    of the application. */

class ApplicationException : public std::runtime_error {
public:
    explicit ApplicationException(const std::string& what_arg = "")
        : std::runtime_error(what_arg) { }
};


/// Application base class.
/** The Application object represents an instance of the running
    application. */

class Application
{
public:
    
    typedef struct {
        uint64_t buf_va;
        uint32_t buf_rkey;
    } pdata_t;

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
        ib.setup();
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
