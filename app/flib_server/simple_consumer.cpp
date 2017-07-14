// Copyright 2015 Dirk Hutter

#include "DualRingBuffer.hpp"
#include "shm_channel_client.hpp"
#include "shm_device_client.hpp"
#include <chrono>
#include <csignal>
#include <iomanip>
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

    shm_device = std::make_shared<flib_shm_device_client>(input_shm);

    // add data source
    if (channel_index < shm_device->num_channels()) {
      data_source.reset(new flib_shm_channel_client(shm_device, channel_index));
    } else {
      throw std::runtime_error("shared memory channel not available");
    }

    // initialize indices
    DualIndex write_index = data_source->get_write_index();
    DualIndex read_index = data_source->get_read_index();
    DualIndex read_index_cached = read_index;
    DualIndex start_index = read_index;
    std::cout << "Starting with microslice index: " << start_index.desc
              << std::endl;

    bool running = true;
    time_point<high_resolution_clock> start, end;
    time_point<high_resolution_clock> tp, now;
    duration<double> delta;
    double throughput = 0;
    double freq = 0;
    uint64_t acc_payload = 0;
    uint64_t acc_payload_cached = 0;

    auto integration_time = seconds(1);
    auto run_time = seconds(10);
    bool run_infinit = true;
    bool analyze = true;
    bool human_readable = true;

    if (!human_readable) {
      std::cout << "total (MB/s) data (MB/s) payload (MB/s) desc (MB/s) "
                   "freq_desc (kHz) avg_size_ms (kB)"
                << std::endl;
    }
    start = high_resolution_clock::now();
    tp = high_resolution_clock::now();
    while (signal_status == 0 && running) {
      write_index = data_source->get_write_index();
      if (write_index.desc > read_index.desc) {
        if (analyze) {
          while (write_index.desc > read_index.desc) {
            acc_payload += data_source->desc_buffer().at(read_index.desc).size;
            read_index.desc += 1;
          }
        }
        read_index = write_index;
        data_source->set_read_index(read_index);
      }
      now = high_resolution_clock::now();
      if (now > (tp + integration_time)) {
        delta = (now - tp);
        auto index_delta = read_index - read_index_cached;
        auto payload_delta = acc_payload - acc_payload_cached;
        if (human_readable) {
          std::cout << "Throughput total: "
                    << (index_delta.data +
                        index_delta.desc * sizeof(fles::MicrosliceDescriptor)) /
                           delta.count() / 1000000.
                    << " MB/s";
          std::cout << ", data: " << index_delta.data / delta.count() / 1000000.
                    << " MB/s";
          std::cout << ", payload: " << payload_delta / delta.count() / 1000000.
                    << " MB/s";
          std::cout << ", desc: "
                    << index_delta.desc * sizeof(fles::MicrosliceDescriptor) /
                           delta.count() / 1000000.
                    << " MB/s";
          std::cout << " Freq. desc: "
                    << index_delta.desc / delta.count() / 1000. << " kHz";
          std::cout << " Avg. ms size: "
                    << static_cast<double>(index_delta.data) /
                           index_delta.desc / 1000.
                    << " kB" << std::endl;
        } else {
          std::cout << (index_delta.data +
                        index_delta.desc * sizeof(fles::MicrosliceDescriptor)) /
                           delta.count() / 1000000.
                    << " ";
          std::cout << index_delta.data / delta.count() / 1000000. << " ";
          std::cout << payload_delta / delta.count() / 1000000. << " ";
          std::cout << index_delta.desc * sizeof(fles::MicrosliceDescriptor) /
                           delta.count() / 1000000.
                    << " ";
          std::cout << index_delta.desc / delta.count() / 1000. << " ";
          std::cout << static_cast<double>(index_delta.data) /
                           index_delta.desc / 1000.
                    << std::endl;
        }
        read_index_cached = read_index;
        acc_payload_cached = acc_payload;
        tp = now;
      }
      if (!run_infinit && now > (start + run_time)) {
        running = false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    end = high_resolution_clock::now();

    delta = end - start;
    auto index_delta = read_index - start_index;

    throughput = index_delta.data / delta.count();
    freq = index_delta.desc / delta.count();

    //  std::cout << "Runtime: " << duration_cast<seconds>(delta).count() <<
    //  "s"<<
    //  std::endl;
    std::cout << "Total runtime: " << delta.count() << "s" << std::endl;
    std::cout << "Microslices processed: " << index_delta.desc << std::endl;
    std::cout << "Bytes processed: " << index_delta.data << std::endl;
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
