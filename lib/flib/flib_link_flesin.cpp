/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include <cassert>
#include <memory>
#include <arpa/inet.h> // ntohl

#include <flib_link_flesin.hpp>

namespace flib {

flib_link_flesin::flib_link_flesin(size_t link_index,
                                   pda::device* dev,
                                   pda::pci_bar* bar)
    : flib_link(link_index, dev, bar) {
  m_rfflim = std::unique_ptr<register_file>(new register_file_bar(
      bar, (m_base_addr + (1 << RORC_DMA_CMP_SEL) + (1 << 5))));
}

//////*** Readout ***//////

void flib_link_flesin::enable_readout(bool enable) { (void)enable; }

void flib_link_flesin::set_pgen_rate(float val) {
  assert(val >= 0);
  assert(val <= 1);
  uint16_t reg_val =
      static_cast<uint16_t>(static_cast<float>(UINT16_MAX) * (1.0 - val));
  m_rfgtx->set_reg(RORC_REG_GTX_MC_GEN_CFG,
                   static_cast<uint32_t>(reg_val) << 16,
                   0xFFFF0000);
}

//////*** FLIM Configuration and Status***//////

void flib_link_flesin::set_flim_ready_for_data(bool enable) {
  m_rfflim->set_bit(RORC_REG_LINK_FLIM_CFG, 0, enable);
}
void flib_link_flesin::set_film_data_source(flim_data_source_t sel) {
  m_rfflim->set_bit(RORC_REG_LINK_FLIM_CFG, 1, sel);
}

void flib_link_flesin::set_flim_start_idx(uint64_t idx) {
  m_rfflim->set_mem(RORC_REG_LINK_MC_PACKER_CFG_IDX_L, &idx, 2);
  m_rfflim->set_bit(RORC_REG_LINK_FLIM_CFG, 31, true); // pulse bit
}

uint64_t flib_link_flesin::get_flim_mc_idx() {
  uint64_t idx;
  m_rfflim->get_mem(RORC_REG_LINK_MC_INDEX_L, &idx, 2);
  return idx;
}

bool flib_link_flesin::get_flim_pgen_present() {
  return m_rfflim->get_bit(RORC_REG_LINK_FLIM_STS, 0);
}

flim_sts_t flib_link_flesin::get_flim_sts() {
  uint32_t reg = m_rfflim->get_reg(RORC_REG_LINK_FLIM_STS);
  flim_sts_t sts;
  sts.hard_err = (reg & (1 << 1));
  sts.soft_err = (reg & (1 << 2));
  return sts;
}

void flib_link_flesin::set_flim_pgen_mc_size(uint32_t size) {
  m_rfflim->set_reg(RORC_REG_LINK_MC_PGEN_MC_SIZE, size);
}

void flib_link_flesin::set_flim_pgen_ids(uint16_t eq_id) {
  m_rfflim->set_reg(RORC_REG_LINK_MC_PGEN_IDS, eq_id);
}

void flib_link_flesin::set_flim_pgen_rate(float val) {
  assert(val >= 0);
  assert(val <= 1);
  uint16_t reg_val =
      static_cast<uint16_t>(static_cast<float>(UINT16_MAX) * (1.0 - val));
  m_rfgtx->set_reg(RORC_REG_LINK_MC_PGEN_CFG,
                   static_cast<uint32_t>(reg_val) << 16,
                   0xFFFF0000);
}

void flib_link_flesin::set_flim_pgen_enable(bool enable) {
  m_rfflim->set_bit(RORC_REG_LINK_MC_PGEN_CFG, 0, enable);
}

void flib_link_flesin::reset_flim_pgen_mc_pending() {
  m_rfflim->set_bit(RORC_REG_LINK_MC_PGEN_CFG, 1, true); // pulse bit
}

uint32_t flib_link_flesin::get_flim_pgen_mc_pending() {
  return m_rfflim->get_reg(RORC_REG_LINK_MC_PGEN_MC_PENDING);
}

//////*** FLIM Test and Debug ***//////

void flib_link_flesin::set_flim_testreg(uint32_t data) {
  m_rfflim->set_reg(RORC_REG_LINK_TEST, data);
}

uint32_t flib_link_flesin::get_flim_testreg() {
  return m_rfflim->get_reg(RORC_REG_LINK_TEST);
}

void flib_link_flesin::set_flim_debug_out(bool enable) {
  m_rfflim->set_bit(RORC_REG_LINK_TEST, 0, enable);
}

//////*** FLIM Hardware Info ***//////

uint16_t flib_link_flesin::flim_hardware_ver() {
  return (static_cast<uint16_t>(m_rfflim->get_reg(0) >> 16));
  // RORC_REG_LINK_FLIM_HW_INFO
}

uint16_t flib_link_flesin::flim_hardware_id() {
  return (static_cast<uint16_t>(m_rfflim->get_reg(0)));
  // RORC_REG_LINK_FLIM_HW_INFO
}

boost::posix_time::ptime flib_link_flesin::flim_build_date() {
  time_t time =
      (static_cast<time_t>(m_rfflim->get_reg(RORC_REG_FLIM_BUILD_DATE_L)) |
       (static_cast<uint64_t>(m_rfflim->get_reg(RORC_REG_FLIM_BUILD_DATE_H))
        << 32));
  boost::posix_time::ptime t = boost::posix_time::from_time_t(time);
  return t;
}

std::string flib_link_flesin::flim_build_host() {
  uint32_t host[4];
  host[0] = ntohl(m_rfflim->get_reg(RORC_REG_FLIM_BUILD_HOST_3));
  host[1] = ntohl(m_rfflim->get_reg(RORC_REG_FLIM_BUILD_HOST_2));
  host[2] = ntohl(m_rfflim->get_reg(RORC_REG_FLIM_BUILD_HOST_1));
  host[3] = ntohl(m_rfflim->get_reg(RORC_REG_FLIM_BUILD_HOST_0));
  return std::string(reinterpret_cast<const char*>(host));
}

std::string flib_link_flesin::flim_build_user() {
  uint32_t user[4];
  user[0] = ntohl(m_rfflim->get_reg(RORC_REG_FLIM_BUILD_USER_3));
  user[1] = ntohl(m_rfflim->get_reg(RORC_REG_FLIM_BUILD_USER_2));
  user[2] = ntohl(m_rfflim->get_reg(RORC_REG_FLIM_BUILD_USER_1));
  user[3] = ntohl(m_rfflim->get_reg(RORC_REG_FLIM_BUILD_USER_0));
  return std::string(reinterpret_cast<const char*>(user));
}

struct flim_build_info_t flib_link_flesin::flim_build_info() {
  flim_build_info_t info;

  info.date = flim_build_date();
  info.host = flim_build_host();
  info.user = flim_build_user();
  info.rev[0] = m_rfflim->get_reg(RORC_REG_FLIM_BUILD_REV_0);
  info.rev[1] = m_rfflim->get_reg(RORC_REG_FLIM_BUILD_REV_1);
  info.rev[2] = m_rfflim->get_reg(RORC_REG_FLIM_BUILD_REV_2);
  info.rev[3] = m_rfflim->get_reg(RORC_REG_FLIM_BUILD_REV_3);
  info.rev[4] = m_rfflim->get_reg(RORC_REG_FLIM_BUILD_REV_4);
  info.hw_ver = flim_hardware_ver();
  info.clean = (m_rfflim->get_reg(RORC_REG_FLIM_BUILD_FLAGS) & 0x1);
  info.repo = (m_rfflim->get_reg(RORC_REG_FLIM_BUILD_FLAGS) & 0x6) >> 1;
  return info;
}

std::string flib_link_flesin::print_flim_build_info() {
  flim_build_info_t build = flim_build_info();
  std::stringstream ss;
  ss << "Build Date:     " << build.date << " UTC" << std::endl
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

} // namespace
