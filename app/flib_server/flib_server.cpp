// Copyright 2015 Dirk Hutter

#include <csignal>

#include "shm_device_server.hpp"

namespace
{
  volatile std::sig_atomic_t signal_status = 0;
}

static void signal_handler(int sig) {
  signal_status = sig;
}

shm_device_server server(22, 22, &signal_status);

static void stop_server(int sig) {
  signal_status = sig;
  server.stop();
}


int main(int argc, char* argv[])
{

//  std::signal(SIGINT, signal_handler);
//  std::signal(SIGTERM, signal_handler);
  std::signal(SIGINT, stop_server);
  std::signal(SIGTERM, stop_server);

  //  parameters par(argc, argv);
  //shm_device_server server(22, 22, &signal);


  server.run();
  
  return 0;
}




