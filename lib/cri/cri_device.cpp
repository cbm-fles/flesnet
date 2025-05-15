/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "cri_device.hpp"
#include "cri_channel.hpp"
#include "cri_registers.hpp"
#include "data_structures.hpp"
#include "pda/device.hpp"
#include "pda/device_operator.hpp"
#include "pda/pci_bar.hpp"
#include "register_file_bar.hpp"
#include <arpa/inet.h> // ntohl
#include <ctime>
#include <iomanip>
#include <memory>

namespace cri {

cri_device::cri_device(int device_nr) {
  m_device_op = std::make_unique<pda::device_operator>();
  m_device = std::make_unique<pda::device>(m_device_op.get(), device_nr);
  init();
}

cri_device::cri_device(uint8_t bus, uint8_t device, uint8_t function) {
  m_device_op = nullptr;
  m_device = std::make_unique<pda::device>(bus, device, function);
  init();
}

cri_device::~cri_device() = default;

void cri_device::init() {
  m_bar = std::make_unique<pda::pci_bar>(m_device.get(), 0);
  // register file access
  m_register_file = std::make_unique<register_file_bar>(m_bar.get(), 0);
  // enforce correct magic number and hw version
  check_magic_number();
  m_hardware_version =
      m_register_file->get_reg(0) >> 16; // CRI_REG_HARDWARE_INFO;
  check_hw_ver(hw_ver_table);
  // create channel objects
  uint8_t num_channels = number_of_hw_channels();
  for (size_t i = 0; i < num_channels; i++) {
    m_channel.push_back(
        std::make_unique<cri_channel>(i, m_device.get(), m_bar.get()));
  }
}

bool cri_device::check_hw_ver(hw_ver_table_t hw_ver_table) const {
  bool match = false;

  // check if version of hardware is part of suported versions
  for (auto* it = hw_ver_table.begin(); it != hw_ver_table.end() && !match;
       ++it) {
    if (m_hardware_version == *it) {
      match = true;
    }
  }
  if (!match) {
    std::stringstream msg;
    msg << "Hardware - libcri version missmatch! CRI ver: "
        << m_hardware_version;
    throw CriException(msg.str());
  }

  // INFO: disable check to allow 'mixed' hw headers
  // check if version of hardware matches or is older than version of header
  if (m_hardware_version > CRI_C_HARDWARE_VERSION) {
    match = false;
  }
  if (!match) {
    std::stringstream msg;
    msg << "Header file version missmatch! CRI ver: " << m_hardware_version;
    throw CriException(msg.str());
  }
  return match;
}

void cri_device::enable_mc_cnt(bool enable) {
  m_register_file->set_bit(CRI_REG_MC_CNT_CFG_L, 31, enable);
}

void cri_device::set_pgen_mc_size(uint32_t mc_size_ticks) {
  uint32_t mc_size_ns = mc_size_ticks * pgen_base_size_ns;
  // time registers are 31 bit wide, we don't check input here
  m_register_file->set_reg(CRI_REG_MC_CNT_CFG_L, mc_size_ticks, 0x7FFFFFFF);
  m_register_file->set_reg(CRI_REG_MC_CNT_CFG_H, mc_size_ns, 0x7FFFFFFF);
}

uint32_t cri_device::get_pgen_mc_size_ns() {
  return (m_register_file->get_reg(CRI_REG_MC_CNT_CFG_H) & 0x7FFFFFFF);
}

void cri_device::set_pgen_start_time(uint64_t start_time) {
  m_register_file->set_mem(CRI_REG_MC_CNT_TIME_L, &start_time, 2);
}

uint8_t cri_device::number_of_hw_channels() {
  return (m_register_file->get_reg(CRI_REG_N_CHANNELS) & 0xFF);
}

uint16_t cri_device::hardware_version() const { return m_hardware_version; }

time_t cri_device::build_date() {
  time_t time = (static_cast<time_t>(
      m_register_file->get_reg(CRI_REG_BUILD_DATE_L) |
      (static_cast<uint64_t>(m_register_file->get_reg(CRI_REG_BUILD_DATE_H))
       << 32)));
  return time;
}

std::string cri_device::build_host() {
  uint32_t host[4];
  host[0] = ntohl(m_register_file->get_reg(CRI_REG_BUILD_HOST_3)); // NOLINT
  host[1] = ntohl(m_register_file->get_reg(CRI_REG_BUILD_HOST_2)); // NOLINT
  host[2] = ntohl(m_register_file->get_reg(CRI_REG_BUILD_HOST_1)); // NOLINT
  host[3] = ntohl(m_register_file->get_reg(CRI_REG_BUILD_HOST_0)); // NOLINT
  return {reinterpret_cast<const char*>(host)};
}

std::string cri_device::build_user() {
  uint32_t user[4];
  user[0] = ntohl(m_register_file->get_reg(CRI_REG_BUILD_USER_3)); // NOLINT
  user[1] = ntohl(m_register_file->get_reg(CRI_REG_BUILD_USER_2)); // NOLINT
  user[2] = ntohl(m_register_file->get_reg(CRI_REG_BUILD_USER_1)); // NOLINT
  user[3] = ntohl(m_register_file->get_reg(CRI_REG_BUILD_USER_0)); // NOLINT
  return {reinterpret_cast<const char*>(user)};
}

struct build_info_t cri_device::build_info() {
  build_info_t info;

  info.date = build_date();
  info.host = build_host();
  info.user = build_user();
  info.rev[0] = m_register_file->get_reg(CRI_REG_BUILD_REV_0);
  info.rev[1] = m_register_file->get_reg(CRI_REG_BUILD_REV_1);
  info.rev[2] = m_register_file->get_reg(CRI_REG_BUILD_REV_2);
  info.rev[3] = m_register_file->get_reg(CRI_REG_BUILD_REV_3);
  info.rev[4] = m_register_file->get_reg(CRI_REG_BUILD_REV_4);
  info.hw_ver = hardware_version();
  info.clean = ((m_register_file->get_reg(CRI_REG_BUILD_FLAGS) & 0x1) != 0u);
  info.repo = (m_register_file->get_reg(CRI_REG_BUILD_FLAGS) & 0x6) >> 1;
  return info;
}

std::string cri_device::print_build_info() {
  build_info_t build = build_info();

  std::stringstream ss;
  ss << "CRI Build Info:" << std::endl
     << "Build Date:     "
     << std::put_time(std::localtime(&build.date), "%c %Z UTC%z") << std::endl
     << "Build Source:   " << build.user << "@" << build.host << std::endl;
  switch (build.repo) {
  case 1:
    ss << "Build from a git repository" << std::endl
       << "Repository Revision: " << std::hex << std::setfill('0')
       << std::setw(8) << build.rev[4] << std::setfill('0') << std::setw(8)
       << build.rev[3] << std::setfill('0') << std::setw(8) << build.rev[2]
       << std::setfill('0') << std::setw(8) << build.rev[1] << std::setfill('0')
       << std::setw(8) << build.rev[0] << std::endl;
    break;
  case 2:
    ss << "Build from a svn repository" << std::endl
       << "Repository Revision: " << std::dec << build.rev[0] << std::endl;
    break;
  default:
    ss << "Build from a unknown repository" << std::endl;
    break;
  }
  if (build.clean) {
    ss << "Repository Status:   clean " << std::endl;
  } else {
    ss << "Repository Status:   NOT clean " << std::endl;
  }
  ss << "Hardware Version:    " << std::dec << build.hw_ver;
  return ss.str();
}

std::string cri_device::print_devinfo() {
  std::stringstream ss;
  ss << std::hex << std::setw(2) << std::setfill('0')
     << static_cast<unsigned>(m_device->bus()) << ":" << std::setw(2)
     << std::setfill('0') << static_cast<unsigned>(m_device->slot()) << "."
     << static_cast<unsigned>(m_device->func());
  return ss.str();
}

std::chrono::seconds cri_device::uptime() {
  std::chrono::duration<double, std::ratio<1, pci_clk>> uptime(
      static_cast<uint64_t>(m_register_file->get_reg(CRI_REG_UPTIME)) << 24);
  return std::chrono::duration_cast<std::chrono::seconds>(uptime);
}

std::string cri_device::print_uptime() {
  using namespace std::chrono;
  using days = duration<int, std::ratio<86400>>; // INFO: only needed < C++20
  auto s = uptime();
  auto d = duration_cast<days>(s);
  s -= d;
  auto h = duration_cast<hours>(s);
  s -= h;
  auto m = duration_cast<minutes>(s);
  s -= m;
  std::stringstream ss;
  ss.fill('0');
  ss << d.count() << "d:" << std::setw(2) << h.count() << "h:" << std::setw(2)
     << m.count() << "m:" << std::setw(2) << s.count() << 's';
  return ss.str();
}

std::string cri_device::print_version_warning() const {
  std::stringstream ss;
  if (m_hardware_version != hw_ver_table.back()) {
    ss << "Hardware version is outdated. "
       << "Latest version is " << hw_ver_table.back() << ".";
  }
  return ss.str();
}

bool cri_device::check_build() {
  // check if latest hardware version and clean build
  return (m_hardware_version == hw_ver_table.back()) && build_info().clean;
}

size_t cri_device::number_of_channels() { return m_channel.size(); }

std::vector<cri_channel*> cri_device::channels() {
  std::vector<cri_channel*> channels;
  for (auto& l : m_channel) {
    channels.push_back(l.get());
  }
  return channels;
}

cri_channel* cri_device::channel(size_t n) { return m_channel.at(n).get(); }

void cri_device::id_led(bool enable) {
  m_register_file->set_bit(CRI_REG_FLIM_CFG, 0, enable);
}

void cri_device::set_testreg(uint32_t data) {
  m_register_file->set_reg(CRI_REG_TESTREG_DEVICE, data);
}

uint32_t cri_device::get_testreg() {
  return m_register_file->get_reg(CRI_REG_TESTREG_DEVICE);
}

//////*** Performance Counters ***//////

// synchonously capture and/or reset all perf counters
void cri_device::set_perf_cnt(bool capture, bool reset) {
  uint32_t reg = 0;
  reg |= ((static_cast<int>(capture) << 1) | static_cast<int>(reset));
  m_register_file->set_reg(CRI_REG_PCI_PERF_CFG, reg, 0x3);
}

// get perf interval in clock cycles
uint32_t cri_device::get_perf_cycles() {
  return m_register_file->get_reg(CRI_REG_PCI_PERF_CYCLE);
}

// words accepted by the pcie core (cycles)
uint32_t cri_device::get_pci_trans() {
  return m_register_file->get_reg(CRI_REG_PCI_PERF_DMA_TRANS);
}

// word not accepted due to back pressure from pcie core (cycles)
uint32_t cri_device::get_pci_stall() {
  return m_register_file->get_reg(CRI_REG_PCI_PERF_DMA_STALL);
}

// back pressure from pcie core which DMA idle (cycles)
uint32_t cri_device::get_pci_busy() {
  return m_register_file->get_reg(CRI_REG_PCI_PERF_DMA_BUSY);
}

// max. duration of continious back pressure from pcie core (us)
float cri_device::get_pci_max_stall() {
  float pci_max_stall = static_cast<float>(
      m_register_file->get_reg(CRI_REG_PCI_PERF_DMA_MAX_NRDY));
  return pci_max_stall * (1.0 / pci_clk) * 1E6;
}

cri_device::dev_perf_t cri_device::get_perf() {
  dev_perf_t perf;
  // capture and rest perf counters
  set_perf_cnt(true, true);
  // return shadowed counters
  perf.cycles = get_perf_cycles();
  perf.pci_trans = get_pci_trans();
  perf.pci_stall = get_pci_stall();
  perf.pci_busy = get_pci_busy();
  perf.pci_max_stall = get_pci_max_stall();
  return perf;
}

register_file_bar* cri_device::rf() const { return m_register_file.get(); }

bool cri_device::check_magic_number() {
  // CRI_REG_HARDWARE_INFO
  if ((m_register_file->get_reg(0) & 0xFFFF) != CRI_C_HARDWARE_ID) {
    std::stringstream msg;
    msg << "Cannot read magic number! \n Try to reinitialize CRI";
    throw CriException(msg.str());
  }
  return true;
}
} // namespace cri
