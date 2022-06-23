/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "Monitor.hpp"
#include "cri.hpp"
#include "device_operator.hpp"
#include "fles_ipc/System.hpp"
#include <boost/program_options.hpp>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

// measurement interval (equals output interval)
constexpr uint32_t interval_ms = 1000;
constexpr bool clear_screen = true;

struct pci_perf_data_t {
  uint64_t cycle_cnt;
  uint64_t pci_stall;
  uint64_t pci_trans;
};

int s_interrupted = 0;
static void s_signal_handler(int signal_value) {
  (void)signal_value;
  s_interrupted = 1;
}

static void s_catch_signals() {
  struct sigaction action {};
  action.sa_handler = s_signal_handler;
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  sigaction(SIGABRT, &action, nullptr);
  sigaction(SIGTERM, &action, nullptr);
  sigaction(SIGINT, &action, nullptr);
}

namespace po = boost::program_options;

int main(int argc, char* argv[]) {
  std::string monitor_uri;
  bool detailed_stats = false;
  po::options_description desc("Allowed options");
  auto desc_add = desc.add_options();
  desc_add("help,h", "produce help message");
  desc_add("desc", "show output description");
  desc_add("verbose,v", po::value<bool>(&detailed_stats)->implicit_value(true),
           "show detailed statistics");
  desc_add("monitor,m",
           po::value<std::string>(&monitor_uri)
               ->value_name("<uri>")
               ->implicit_value("influx1:login:8086:cri_status"),
           "publish CRI status to InfluxDB (or \"file:cout\" for "
           "console output)");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
  if (vm.count("help") != 0u) {
    std::cout << desc << std::endl;
    exit(EXIT_SUCCESS);
  }
  if (vm.count("desc") != 0u) {
    std::cout
        << "Displays status and performance counters for all CRI channelss.\n"
           "Per device counters:\n"
           "idle:     PCIe interface is idle (ratio)\n"
           "stall:    back pressure on PCIe interface from host (ratio)\n"
           "trans:    data is transmitted via PCIe interface (ratio)\n"
           "Per channel status/counters:\n"
           "ch:       cri/channle\n"
           "src:      choosen data source\n"
           "en:       readout enabled\n"
           "dma_t:    transmission to dma mux (ratio)\n"
           "dma_s:    stall from dma mux (ratio)\n"
           "data_s:   stall from full data buffer (ratio)\n"
           "desc_s:   stall from full desc buffer (ratio)\n"
           "rate:     ms processing rate (Hz*)\n"
           "* Based on the assumption that the PCIe clock is exactly 100 "
           "MHz.\n"
           "  This may not be true in case of PCIe spread-spectrum "
           "clocking.\n";
    std::cout << std::endl;
    return EXIT_SUCCESS;
  }

  bool console = true;
  std::unique_ptr<cbm::Monitor> monitor;
  if (!monitor_uri.empty()) {
    console = false;
    try {
      monitor = std::make_unique<cbm::Monitor>(monitor_uri);
    } catch (std::exception& e) {
      std::cerr << e.what() << std::endl;
      return EXIT_FAILURE;
    }
  }
  auto hostname = fles::system::current_hostname();

  s_catch_signals();
  try {
    std::unique_ptr<pda::device_operator> dev_op(new pda::device_operator);
    std::vector<std::unique_ptr<cri::cri_device>> cris;
    uint64_t num_dev = dev_op->device_count();

    for (size_t i = 0; i < num_dev; ++i) {
      cris.push_back(std::make_unique<cri::cri_device>(i));
    }

    // set measurement interval for device and all channels
    for (auto& cri : cris) {
      cri->set_perf_cnt(false, true); // reset counters
      for (auto& channel : cri->channels()) {
        channel->set_perf_cnt(false, true);     // reset counters
        channel->set_perf_gtx_cnt(false, true); // reset counters
      }
    }

    std::cout << "Starting measurements" << std::endl;
    if (console && clear_screen) {
      std::cout << "\x1B[2J" << std::flush;
    }

    // main output loop
    size_t loop_cnt = 0;
    auto start = std::chrono::steady_clock::now();
    while (s_interrupted == 0) {
      if (console) {
        if (clear_screen) {
          std::cout << "\x1B[H" << std::flush;
        }
        std::cout << "Measurement " << loop_cnt << ":" << std::endl;
      }
      std::string measurement;
      size_t j = 0;
      for (auto& cri : cris) {
        cri::cri_device::dev_perf_t dev_perf = cri->get_perf();

        // check overflow
        if (dev_perf.cycles == 0xFFFFFFFF) {
          if (console) {
            std::cout << "CRI " << j << " (" << cri->print_devinfo() << ")"
                      << std::endl;
            std::cout << "PCIe stats counter overflow" << std::endl;
          }
          continue;
        }

        float cycles = static_cast<float>(dev_perf.cycles);
        float pci_trans = dev_perf.pci_trans / cycles;
        float pci_stall = dev_perf.pci_stall / cycles;
        float pci_busy = dev_perf.pci_busy / cycles;
        float pci_idle = 1 - pci_trans - pci_stall - pci_busy;
        float pci_max_stall = dev_perf.pci_max_stall;
        float pci_throughput = pci_trans * cri::pci_clk * 32;

        if (console) {
          std::cout << "CRI " << j << " (" << cri->print_devinfo() << ")"
                    << std::endl;
          std::cout << std::setprecision(4) << "PCIe idle " << std::setw(9)
                    << pci_idle << "   stall " << std::setw(9) << pci_stall
                    << " (max. " << std::setw(5) << pci_max_stall << " us)"
                    << "   trans " << std::setw(9) << pci_trans
                    << "   raw rate " << std::setw(5) << pci_throughput / 1e6
                    << " MB/s    " << std::endl;
        }
        if (monitor) {
          monitor->QueueMetric(
              "dev_status", {{"host", hostname}, {"cri", cri->print_devinfo()}},
              {{"cycles", dev_perf.cycles},
               {"busy", pci_busy},
               {"stall", pci_stall},
               {"trans=", pci_trans}});
        }

        ++j;
      }
      if (console) {
        std::cout << std::endl;

        std::cout << " ch       src  en      MB/s       kHz      mc_t      "
                     "mc_s     dma_t     dma_s    data_s    desc_s\n";
      }
      j = 0;
      for (auto& cri : cris) {
        size_t num_channels = cri->number_of_hw_channels();
        std::vector<cri::cri_channel*> channels = cri->channels();

        std::stringstream ss;
        for (size_t i = 0; i < num_channels; ++i) {
          cri::cri_channel::ch_perf_t perf = channels.at(i)->get_perf();
          cri::cri_channel::ch_perf_gtx_t perf_gtx =
              channels.at(i)->get_perf_gtx();
          bool ready_for_data = channels.at(i)->get_ready_for_data();

          // check overflow
          if (perf.cycles == 0xFFFFFFFF || perf_gtx.cycles == 0xFFFFFFFF) {
            if (console) {
              ss << std::setw(2) << j << "/" << i << "  ";
              ss << std::setw(8) << channels.at(i)->data_source() << "  ";
              ss << "stats counter overflow";
              ss << "\n";
            }
            continue;
          }

          float cycles = static_cast<float>(perf.cycles);
          float dma_trans = perf.dma_trans / cycles;
          float dma_stall = perf.dma_stall / cycles;
          float dma_busy = perf.dma_busy / cycles;
          float data_buf_stall = perf.data_buf_stall / cycles;
          float desc_buf_stall = perf.desc_buf_stall / cycles;
          float microslice_rate = perf.microslice_cnt / (cycles / cri::pkt_clk);

          float cycles_gtx = static_cast<float>(perf_gtx.cycles);
          float mc_trans = perf_gtx.mc_trans / cycles_gtx;
          float mc_stall = perf_gtx.mc_stall / cycles_gtx;
          float mc_busy = perf_gtx.mc_busy / cycles_gtx;
          float mc_throughput = mc_trans * cri::gtx_clk * 8;

          if (console) {
            ss << std::setw(1) << j << "/" << i << "  ";
            ss << std::setw(8) << channels.at(i)->data_source() << "  ";
            ss << std::setw(2) << ready_for_data << "  ";
            // perf counters
            ss << std::setprecision(5);
            ss << std::setw(8) << mc_throughput / 1e6 << "  ";
            ss << std::setw(8) << microslice_rate / 1e3 << "  ";
            ss << std::setprecision(3); // percision + 5 = width
            ss << std::setw(8) << mc_trans * 100 << "  ";
            ss << std::setw(8) << mc_stall * 100 << "  ";
            ss << std::setw(8) << dma_trans * 100 << "  ";
            ss << std::setw(8) << dma_stall * 100 << "  ";
            ss << std::setw(8) << data_buf_stall * 100 << "  ";
            ss << std::setw(8) << desc_buf_stall * 100 << "  ";
            ss << "\n";
          }

          if (monitor) {
            monitor->QueueMetric(
                "ch_status",
                {{"host", hostname},
                 {"cri", cri->print_devinfo()},
                 {"ch", std::to_string(i)}},
                {{"data_src", static_cast<int>(channels.at(i)->data_source())},
                 {"enable", ready_for_data},
                 {"throughput", mc_throughput},
                 {"rate", microslice_rate},
                 {"mc_trans", mc_trans},
                 {"mc_stall", mc_stall},
                 {"mc_busy", mc_busy},
                 {"dma_trans", dma_trans},
                 {"dma_stall", dma_stall},
                 {"dma_busy", dma_busy},
                 {"data_buf_stall", data_buf_stall},
                 {"desc_buf_stall", desc_buf_stall},
                 {"cycles_dma", perf.cycles},
                 {"cycles_mc", perf_gtx.cycles}});
          }
        }
        if (console) {
          std::cout << ss.str() << std::endl;
        }

        ++j;
      }
      // sleep will be canceled by signals (which is handy in our case)
      // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66803
      ++loop_cnt;
      std::this_thread::sleep_until(
          start + loop_cnt * std::chrono::milliseconds(interval_ms));
    }
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
