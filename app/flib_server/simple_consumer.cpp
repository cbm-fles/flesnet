// Copyright 2015 Dirk Hutter

#include "DualRingBuffer.hpp"
#include "shm_channel_client.hpp"
#include "shm_device_client.hpp"
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

using namespace std;
using namespace std::chrono;

namespace {
volatile std::sig_atomic_t signal_status = 0;
}

static void signal_handler(int sig) { signal_status = sig; }

int main(int argc, char* argv[]) {
  try {

    if (argc != 2) {
      cerr << "Provide channel index as argument." << endl;
      return EXIT_FAILURE;
    }
    size_t channel_index = atoi(argv[1]);
    std::string input_shm = "flib_shared_memory";

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    //  auto dev = make_shared<flib_shm_device_client>("flib_shared_memory");
    //
    //  size_t num_channels = dev->num_channels();
    //  std::vector<std::unique_ptr<flib_shm_channel_client>> ch;
    //  for (size_t i = 0; i < num_channels; ++i) {
    //    ch.push_back(std::unique_ptr<flib_shm_channel_client>(
    //        new flib_shm_channel_client(dev, i)));
    //  }
    //
    //  cout << dev->num_channels() << endl;
    //  cout << ch.size() << endl;

    std::unique_ptr<InputBufferReadInterface> data_source;
    std::shared_ptr<flib_shm_device_client> shm_device;
    DualIndex read_index = {0, 0};
    DualIndex read_index_cached = {0, 0};
    DualIndex write_index = {0, 0};

    shm_device = std::make_shared<flib_shm_device_client>(input_shm);

    if (channel_index < shm_device->num_channels()) {
      data_source.reset(new flib_shm_channel_client(shm_device, channel_index));
    } else {
      throw std::runtime_error("shared memory channel not available");
    }

    time_point<high_resolution_clock> start, end;
    time_point<high_resolution_clock> tp, now;
    duration<double> delta;
    double throughput = 0;
    double freq = 0;

    start = high_resolution_clock::now();
    tp = high_resolution_clock::now();
    while (signal_status == 0) {
      write_index = data_source->get_write_index();
      if (write_index.desc > read_index.desc) {
        // discard microslices
        read_index = write_index;
        data_source->set_read_index(read_index);
      }
      now = high_resolution_clock::now();
      if (now > (tp + seconds(1))) {
        delta = (now - tp);
        auto index_delta = read_index - read_index_cached;
        std::cout << "Throughput: "
                  << index_delta.data / delta.count() / 1000000. << " MB/s "
                  << "Freq: " << index_delta.desc / delta.count() / 1000.
                  << " kHz "
                  << "Avg: "
                  << static_cast<double>(index_delta.data) / index_delta.desc /
                         1000.
                  << " kB" << std::endl;
        read_index_cached = read_index;
        tp = now;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    end = high_resolution_clock::now();

    delta = end - start;

    throughput = read_index.data / delta.count();
    freq = read_index.desc / delta.count();

    //  std::cout << "Runtime: " << duration_cast<seconds>(delta).count() <<
    //  "s"<<
    //  std::endl;
    std::cout << "Total runtime: " << delta.count() << "s" << std::endl;
    std::cout << "Microslices processed: " << read_index.desc << std::endl;
    std::cout << "Bytes processed: " << read_index.data << std::endl;
    std::cout << "Throughput: " << throughput / 1000000. << " MB/s"
              << std::endl;
    std::cout << "Freq: " << freq / 1000. << " kHz" << std::endl;

  } catch (std::exception& e) {
    cerr << e.what() << endl;
    return EXIT_FAILURE;
  } catch (...) {
    cerr << "ERROR" << endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
