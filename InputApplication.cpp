/*
 * InputApplication.hpp
 *
 * 2012, Jan de Cuveland
 */

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <boost/thread.hpp>
#include <boost/date_time.hpp>

#include <netdb.h>
#include <arpa/inet.h>
#include <infiniband/arch.h>
#include <rdma/rdma_cma.h>

#include "Application.hpp"
#include "InputBuffer.hpp"
#include "log.hpp"


int
InputApplication::run()
{
    InputBuffer ib;

    std::vector<std::string> services;
    for (unsigned int i = 0; i < _par.compute_nodes().size(); i++)
        services.push_back(boost::lexical_cast<std::string>(BASE_PORT + i));
    
    ib.initiateConnect(_par.compute_nodes(), services);
    ib.handleCmEvents();
    ib.setup();
    boost::thread t1(&InputBuffer::sender_thread, &ib);
    boost::thread t2(&InputBuffer::completionHandler, &ib);

    t1.join();
    t2.join();
    return 0;
}
