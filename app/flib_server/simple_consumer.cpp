// Copyright 2015 Dirk Hutter

#include <csignal>

#include "shm_device_client.hpp"
#include "shm_channel_client.hpp"

#include <iostream>

using namespace std;

namespace {
volatile std::sig_atomic_t signal_status = 0;
}

static void signal_handler(int sig) { signal_status = sig; }

// int main(int argc, char* argv[])
int main() {

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  flib_shm_device_client dev("flib_shared_memory");

  size_t num_channels = dev.num_channels();
  std::vector<std::unique_ptr<flib_shm_channel_client>> ch;
  for (size_t i = 0; i < num_channels; ++i) {
    ch.push_back(std::unique_ptr<flib_shm_channel_client>(
        new flib_shm_channel_client(dev.shm(), i)));
  }

  cout << dev.num_channels() << endl;
  cout << ch.size() << endl;

  //  uint8_t* data_buffer = static_cast<uint8_t*>(ch.at(0)->data_buffer());
  //  uint8_t* desc_buffer = static_cast<uint8_t*>(ch.at(0)->desc_buffer());

  while (signal_status == 0) {
  }

  return 0;
}
