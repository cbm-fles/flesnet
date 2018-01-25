/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "flib.h"
#include "device_operator.hpp"
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

// measurement interval (equals output interval)
constexpr uint32_t interval_ms = 1000;
constexpr bool clear_screen = true;
constexpr bool detailed_stats = true;

std::ostream& operator<<(std::ostream& os, flib::flib_link::data_sel_t sel) {
  switch (sel) {
  case flib::flib_link::rx_disable:
    os << "disable";
    break;
  case flib::flib_link::rx_emu:
    os << "    emu";
    break;
  case flib::flib_link::rx_link:
    os << "   link";
    break;
  case flib::flib_link::rx_pgen:
    os << "   pgen";
    break;
  default:
    os.setstate(std::ios_base::failbit);
  }
  return os;
}

int s_interrupted = 0;
static void s_signal_handler(int signal_value) {
  (void)signal_value;
  s_interrupted = 1;
}

static void s_catch_signals(void) {
  struct sigaction action;
  action.sa_handler = s_signal_handler;
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  sigaction(SIGABRT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGINT, &action, NULL);
}

int main(int argc, char* argv[]) {
  s_catch_signals();

  try {

    // display help if any parameter given
    if (argc != 1) {
      (void)argv;
      std::cout
          << "Displays status and performance counters for all FLIB links.\n"
             "Per FLIB counters:\n"
             "idle:     PCIe interface is idle (ratio)\n"
             "stall:    back pressure on PCIe interface from host (ratio)\n"
             "trans:    data is transmitted via PCIe interface (ratio)\n"
             "Per link status/counters:\n"
             "link:     flib/link\n"
             "data_sel: choosen data source\n"
             "up:       flim channel_up\n"
             "he:       aurora hard_error\n"
             "se:       aurora soft_error\n"
             "eo:       eoe fifo overflow\n"
             "do:       data fifo overflow\n"
             "d_max:    maximum number of words in d_fifo\n"
             "dma_s:    stall from dma mux (ratio)\n"
             "data_s:   stall from full data buffer (ratio)\n"
             "desc_s:   stall from full desc buffer (ratio)\n"
             "bp:       back pressure to link (ratio)\n"
             "rate:     ms processing rate (Hz*)\n"
             "* Based on the assumption that the PCIe clock is exactly 100 "
             "MHz.\n"
             "  This may not be true in case of PCIe spread-spectrum "
             "clocking.\n";
      std::cout << std::endl;
      return EXIT_SUCCESS;
    }

    std::unique_ptr<pda::device_operator> dev_op(new pda::device_operator);
    std::vector<std::unique_ptr<flib::flib_device_flesin>> flibs;
    uint64_t num_dev = dev_op->device_count();
    std::vector<flib::dma_perf_data_t> dma_perf_acc(num_dev);

    for (size_t i = 0; i < num_dev; ++i) {
      flibs.push_back(std::unique_ptr<flib::flib_device_flesin>(
          new flib::flib_device_flesin(i)));
    }

    // set measurement interval for device and all links
    for (auto& flib : flibs) {
      flib->set_perf_interval(interval_ms);
      flib->get_dma_perf(); // dummy read to reset counters
      for (auto& link : flib->links()) {
        link->set_perf_interval(interval_ms);
      }
    }

    std::cout << "Starting measurements" << std::endl;

    // main output loop
    size_t loop_cnt = 0;
    while (s_interrupted == 0) {
      if (clear_screen) {
        std::cout << "\033\143" << std::flush;
      }
      std::cout << "Measurement " << loop_cnt << ":" << std::endl;
      size_t j = 0;
      for (auto& flib : flibs) {
        float pci_stall = flib->get_pci_stall();
        float pci_max_stall = flib->get_pci_max_stall();
        float pci_trans = flib->get_pci_trans();
        float pci_idle = 1 - pci_trans - pci_stall;
        std::cout << "FLIB " << j << " (" << flib->print_devinfo() << ")";
        std::cout << std::setprecision(4) << "  PCIe idle " << std::setw(9)
                  << pci_idle << "   stall " << std::setw(9) << pci_stall
                  << " (max. " << pci_max_stall << " us)"
                  << "   trans " << std::setw(9) << pci_trans << std::endl;

        if (detailed_stats) {
          flib::dma_perf_data_t dma_perf = flib->get_dma_perf();
          dma_perf_acc.at(j).overflow += dma_perf.overflow;
          dma_perf_acc.at(j).cycle_cnt += dma_perf.cycle_cnt;
          dma_perf_acc.at(j).fifo_fill[0] += dma_perf.fifo_fill[0];
          dma_perf_acc.at(j).fifo_fill[1] += dma_perf.fifo_fill[1];
          dma_perf_acc.at(j).fifo_fill[2] += dma_perf.fifo_fill[2];
          dma_perf_acc.at(j).fifo_fill[3] += dma_perf.fifo_fill[3];
          dma_perf_acc.at(j).fifo_fill[4] += dma_perf.fifo_fill[4];
          dma_perf_acc.at(j).fifo_fill[5] += dma_perf.fifo_fill[5];
          dma_perf_acc.at(j).fifo_fill[6] += dma_perf.fifo_fill[6];
          dma_perf_acc.at(j).fifo_fill[7] += dma_perf.fifo_fill[7];

          std::stringstream ss;
          ss << "fill     1/8     2/8     3/8     4/8     5/8     6/8     7/8  "
                "   8/8    merr"
             << std::endl;
          ss << "    ";
          for (size_t i = 0; i <= 7; ++i) {
            ss << " " << std::setw(7) << std::fixed << std::setprecision(3)
               << dma_perf.fifo_fill[i] / float(dma_perf.cycle_cnt) * 100.0;
          }
          ss << " " << std::setw(7) << dma_perf.overflow << std::endl;
          ss << "avg.";
          for (size_t i = 0; i <= 7; ++i) {
            ss << " " << std::setw(7) << std::fixed << std::setprecision(3)
               << dma_perf_acc.at(j).fifo_fill[i] /
                      float(dma_perf_acc.at(j).cycle_cnt) * 100.0;
          }
          ss << " " << std::setw(7) << dma_perf_acc.at(j).overflow << std::endl;
          std::cout << ss.str();
        }

        ++j;
      }
      std::cout << std::endl;

      std::cout
          << "link  data_sel  up  he  se  "
             "eo  do  d_max     dma_s    data_s    desc_s        bp     rate\n";
      j = 0;
      for (auto& flib : flibs) {
        size_t num_links = flib->number_of_hw_links();
        std::vector<flib::flib_link_flesin*> links = flib->links();

        std::stringstream ss;
        for (size_t i = 0; i < num_links; ++i) {
          flib::flib_link_flesin::link_status_t status =
              links.at(i)->link_status();
          flib::flib_link_flesin::link_perf_t perf = links.at(i)->link_perf();

          ss << std::setw(2) << j << "/" << i << "  ";
          ss << std::setw(8) << links.at(i)->data_sel() << "  ";
          // status
          ss << std::setw(2) << status.channel_up << "  ";
          ss << std::setw(2) << status.hard_err << "  ";
          ss << std::setw(2) << status.soft_err << "  ";
          ss << std::setw(2) << status.eoe_fifo_overflow << "  ";
          ss << std::setw(2) << status.d_fifo_overflow << "  ";
          ss << std::setw(5) << status.d_fifo_max_words << "  ";
          // perf counters
          ss << std::setprecision(3); // percision + 5 = width
          ss << std::setw(8) << perf.dma_stall << "  ";
          ss << std::setw(8) << perf.data_buf_stall << "  ";
          ss << std::setw(8) << perf.desc_buf_stall << "  ";
          ss << std::setw(8) << perf.din_full << "  ";
          ss << std::setprecision(7) << std::setw(7) << perf.event_rate << "  ";
          ss << "\n";
        }
        std::cout << ss.str() << std::endl;
        ++j;
      }
      // sleep will be canceled by signals (which is handy in our case)
      // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66803
      std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
      ++loop_cnt;
    }
  } catch (std::exception& e) {
    std::cout << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
