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

try {

//  parameters par(argc, argv);

std::unique_ptr<flib_device> flib =
  std::unique_ptr<flib_device>(new flib_device(0));

shm_device_server server(flib.get(), 27, 23, &signal_status);

server.run();


} catch (std::exception const& e) {
L_(fatal) << "exception: " << e.what();
}

return 0;
}




