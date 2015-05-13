// Copyright 2015 Dirk Hutter

#include <csignal>

//#include "shm_device_client.hpp"

namespace
{
  volatile std::sig_atomic_t signal_status = 0;
}

static void signal_handler(int sig) {
  signal_status = sig;
}



int main(int argc, char* argv[])
{

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  while(signal_status == 0) {
  }
  
  return 0;
}
