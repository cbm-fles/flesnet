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
#include "InputApplication.hpp"
#include "log.hpp"


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
