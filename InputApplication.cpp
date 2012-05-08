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
    InputContext ctx(_par);
    ctx.connect();

    InputBuffer ib(&ctx);
    ib.setup();
    boost::thread t1(&InputBuffer::sender_thread, &ib);
    boost::thread t2(&InputBuffer::completion_thread, &ib);

    t1.join();
    t2.join();
    return 0;
}
