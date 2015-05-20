// Copyright 2015 Dirk Hutter

#include <csignal>

#include "shm_device_client.hpp"

using namespace std;

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


  shm_device_client dev;

  std::vector<shm_channel_client*> ch = dev.channels();

  cout << dev.num_channels() << endl;
  cout << ch.size() << endl;
  
  uint8_t* data_buffer = static_cast<uint8_t*>(ch.at(0)->data_buffer());
  uint8_t* desc_buffer = static_cast<uint8_t*>(ch.at(0)->desc_buffer());

  
  while(signal_status == 0) {
  }
  
  return 0;
}
