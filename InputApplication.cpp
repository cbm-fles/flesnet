/*
 * InputApplication.hpp
 *
 * 2012, Jan de Cuveland
 */

/*
 * connects to server, sends val1 via RDMA write and val2 via send,
 * and receives val1+val2 back from the server.
 *
 * Based on an example by Roland Dreier, http://www.digitalvampire.org/
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
#include "InputApplication.hpp"


int
InputApplication::run()
{
    InputContext ctx;
    ctx.connect(_par.compute_nodes().at(0).c_str());
        
    InputBuffer ib(&ctx);
    ib.setup();
    boost::thread t1(&InputBuffer::sender_loop, &ib);
    boost::thread t2(&InputBuffer::completion_handler, &ib);

    t1.join();
    t2.join();
    return 0;
}
