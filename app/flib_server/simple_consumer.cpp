// Copyright 2015 Dirk Hutter

#include "DualRingBuffer.hpp"
#include "shm_channel_client.hpp"
#include "shm_device_client.hpp"
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <thread>

using namespace std;
using namespace std::chrono;

namespace {
volatile std::sig_atomic_t signal_status = 0;
}

static void signal_handler(int sig) { signal_status = sig; }

int main(int argc, char* argv[]) {
  try {

    // some const config variables
    std::string input_shm = "flib_shared_memory";
    constexpr bool run_infinit = true;
    constexpr auto run_time = seconds(10);
    constexpr auto integration_time = seconds(1);
    constexpr auto sleep_time = milliseconds(10);
    constexpr bool analyze = true;
    constexpr bool human_readable = true;

    std::stringstream help;
    help << "Usage:\n"
         << argv[0] << "            read all channels\n"
         << argv[0] << " <num>      read <num> channels\n"
         << argv[0] << " ch <idx>   read channel <idx>";

    bool mult_channels = true;
    size_t num_channels = 0;
    size_t channel_index = 0;

    if (argc == 2) {
      num_channels = atoi(argv[1]);
      cout << "Using " << num_channels << " channel(s)" << endl;
    } else if (argc == 3) {
      if (std::string(argv[1]) == "ch") {
        mult_channels = false;
        channel_index = atoi(argv[2]);
        cout << "Using channel " << channel_index << endl;
      } else {
        cerr << help.str() << endl;
        return EXIT_FAILURE;
      }
    } else if (argc > 3) {
      cerr << help.str() << endl;
      return EXIT_FAILURE;
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::vector<std::unique_ptr<InputBufferReadInterface>> data_sources;
    std::shared_ptr<flib_shm_device_client> shm_device;

    shm_device = std::make_shared<flib_shm_device_client>(input_shm);

    if (mult_channels && num_channels == 0) {
      // set num to all channels
      num_channels = shm_device->num_channels();
    }
    if (num_channels > shm_device->num_channels()) {
      throw std::runtime_error("too many channels requested");
    }

    if (!mult_channels) {
      if (channel_index < shm_device->num_channels()) {
        data_sources.push_back(std::unique_ptr<InputBufferReadInterface>(
            new flib_shm_channel_client(shm_device, channel_index)));
      } else {
        throw std::runtime_error("shared memory channel not available");
      }
    } else {
      for (size_t i = 0; i < num_channels; ++i) {
        data_sources.push_back(std::unique_ptr<InputBufferReadInterface>(
            new flib_shm_channel_client(shm_device, i)));
      }
    }

    std::vector<DualIndex> write_indexs;
    std::vector<DualIndex> read_indexs;
    std::vector<DualIndex> read_indexs_cached;
    std::vector<DualIndex> start_indexs;
    std::vector<size_t> acc_payloads(data_sources.size(), 0);
    std::vector<size_t> acc_payloads_cached(data_sources.size(), 0);

    for (auto& data_source : data_sources) {
      // initialize indices
      write_indexs.push_back(data_source->get_write_index());
      read_indexs.push_back(data_source->get_read_index());
      read_indexs_cached.push_back(read_indexs.back());
      start_indexs.push_back(read_indexs.back());
      std::cout << "Channel " << start_indexs.size() - 1
                << " is starting with microslice index: "
                << start_indexs.back().desc << std::endl;
    }

    bool running = true;
    time_point<high_resolution_clock> start, end, now;

    std::vector<double> t_total(data_sources.size(), 0);
    std::vector<double> t_data(data_sources.size(), 0);
    std::vector<double> t_payload(data_sources.size(), 0);
    std::vector<double> t_desc(data_sources.size(), 0);
    std::vector<double> f_desc(data_sources.size(), 0);
    std::vector<double> s_ms(data_sources.size(), 0);

    if (!human_readable) {
      std::cout << "ch total (MB/s) data (MB/s) payload (MB/s) desc (MB/s) "
                   "freq_desc (kHz) avg_size_ms (kB)"
                << std::endl;
    }

    start = high_resolution_clock::now();
    time_point<high_resolution_clock> tp(start);

    // main loop
    while (signal_status == 0 && running) {

      // channel loop
      for (size_t i = 0; i < data_sources.size(); ++i) {
        write_indexs.at(i) = data_sources.at(i)->get_write_index();
        if (write_indexs.at(i).desc > read_indexs.at(i).desc) {
          if (analyze) { // loop over ms
            while (write_indexs.at(i).desc > read_indexs.at(i).desc) {
              acc_payloads.at(i) += data_sources.at(i)
                                        ->desc_buffer()
                                        .at(read_indexs.at(i).desc)
                                        .size;
              read_indexs.at(i).desc += 1;
            }
          }
          read_indexs.at(i) = write_indexs.at(i);
          data_sources.at(i)->set_read_index(read_indexs.at(i));
        }
      } // channel loop

      // report all channels en block
      now = high_resolution_clock::now();
      if (now > (tp + integration_time)) {
        for (size_t i = 0; i < data_sources.size(); ++i) {

          duration<double> delta = (now - tp);
          auto index_delta = read_indexs.at(i) - read_indexs_cached.at(i);
          auto payload_delta = acc_payloads.at(i) - acc_payloads_cached.at(i);

          t_total.at(i) =
              (index_delta.data +
               index_delta.desc * sizeof(fles::MicrosliceDescriptor)) /
              delta.count() / 1000000.;
          t_data.at(i) = index_delta.data / delta.count() / 1000000.;
          t_payload.at(i) = payload_delta / delta.count() / 1000000.;
          t_desc.at(i) = index_delta.desc * sizeof(fles::MicrosliceDescriptor) /
                         delta.count() / 1000000.;
          f_desc.at(i) = index_delta.desc / delta.count() / 1000.;
          s_ms.at(i) =
              static_cast<double>(index_delta.data) / index_delta.desc / 1000.;

          std::stringstream ss;
          if (human_readable) {
            ss << "Channel: " << i;
            ss << " Throughput total: " << t_total.at(i) << " MB/s";
            ss << ", data: " << t_data.at(i) << " MB/s";
            ss << ", payload: " << t_payload.at(i) << " MB/s";
            ss << ", desc: " << t_desc.at(i) << " MB/s";
            ss << " Freq. desc: " << f_desc.at(i) << " kHz";
            ss << " Avg. ms size: " << s_ms.at(i) << " kB" << std::endl;
          } else {
            ss << i << " ";
            ss << t_total.at(i) << " ";
            ss << t_data.at(i) << " ";
            ss << t_payload.at(i) << " ";
            ss << t_desc.at(i) << " ";
            ss << f_desc.at(i) << " ";
            ss << s_ms.at(i) << std::endl;
          }
          std::cout << ss.str();
          read_indexs_cached.at(i) = read_indexs.at(i);
          acc_payloads_cached.at(i) = acc_payloads.at(i);
        } // channel report loop

        if (mult_channels) {
          // report channel summery
          double sum_t_total =
              std::accumulate(t_total.begin(), t_total.end(), 0.0);
          double sum_t_data =
              std::accumulate(t_data.begin(), t_data.end(), 0.0);
          double sum_t_payload =
              std::accumulate(t_payload.begin(), t_payload.end(), 0.0);
          double sum_t_desc =
              std::accumulate(t_desc.begin(), t_desc.end(), 0.0);
          double avg_f_desc =
              std::accumulate(f_desc.begin(), f_desc.end(), 0.0) /
              f_desc.size();
          double avg_s_ms =
              std::accumulate(s_ms.begin(), s_ms.end(), 0.0) / s_ms.size();
          std::stringstream ss;

          if (human_readable) {
            ss << "Sum:      ";
            ss << " Throughput total: " << sum_t_total << " MB/s";
            ss << ", data: " << sum_t_data << " MB/s";
            ss << ", payload: " << sum_t_payload << " MB/s";
            ss << ", desc: " << sum_t_desc << " MB/s";
            ss << " Freq. desc: " << avg_f_desc << " kHz";
            ss << " Avg. ms size: " << avg_s_ms << " kB" << std::endl;
          } else {
            ss << "S ";
            ss << sum_t_total << " ";
            ss << sum_t_data << " ";
            ss << sum_t_payload << " ";
            ss << sum_t_desc << " ";
            ss << avg_f_desc << " ";
            ss << avg_s_ms << std::endl;
          }
          std::cout << ss.str();
      }
      tp = now;
      } // report

      if (!run_infinit && now > (start + run_time)) {
        running = false;
      }
      std::this_thread::sleep_for(sleep_time);
    } // main loop

    end = high_resolution_clock::now();

    // run summery
    duration<double> delta = end - start;
    DualIndex index_delta = {0, 0};
    for (size_t i = 0; i < data_sources.size(); ++i) {
      index_delta += read_indexs.at(i) - start_indexs.at(i);
    }
    std::cout << "Run Summery: ";
    std::cout << "Total runtime: " << delta.count() << " s";
    std::cout << " MS processed: " << index_delta.desc;
    std::cout << " Bytes processed: " << index_delta.data;
    std::cout << " Avg total througput: "
              << (index_delta.data +
                  index_delta.desc * sizeof(fles::MicrosliceDescriptor)) /
                     delta.count() / 1000000.
              << " MB/s" << std::endl;

  } catch (std::exception& e) {
    cerr << e.what() << endl;
    return EXIT_FAILURE;
  } catch (...) {
    cerr << "ERROR" << endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
