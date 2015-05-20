// Copyright 2015 Dirk Hutter

#include <csignal>
#include "log.hpp"

#include "shm_device_server.hpp"

namespace
{
  volatile std::sig_atomic_t signal_status = 0;
}

static void signal_handler(int sig) {
  signal_status = sig;
}


int main(int argc, char* argv[])
{

logging::add_console(static_cast<severity_level>(trace));

std::signal(SIGINT, signal_handler);
std::signal(SIGTERM, signal_handler);

//  parameters par(argc, argv);
shm_device_server server(27, 23, &signal_status);

server.run();

return 0;
}




