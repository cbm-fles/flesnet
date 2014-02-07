// Copyright 2014 Dirk Hutter
#ifndef DEVICE_CTRL_CHANNEL_HPP
#define DEVICE_CTRL_CHANNEL_HPP

#include <string>
#include <sstream>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "rorc_registers.h"

namespace flib {

class device_ctrl_channel {
  
public:

  virtual ~device_ctrl_channel() {}
  
  // Interface

  virtual size_t get_mem(uint64_t addr, void *dest, size_t n) = 0;

  virtual size_t set_mem(uint64_t addr, const void *source, size_t n) = 0;

  virtual uint32_t get_reg(uint64_t addr) = 0;
  
  virtual void set_reg(uint64_t addr, uint32_t data) = 0;

  virtual bool get_bit(uint64_t addr, int pos) = 0;

  virtual void set_bit(uint64_t addr, int pos, bool enable) = 0;


  // methods

  void enable_mc_cnt(bool enable) {
    set_bit(RORC_REG_MC_CNT_CFG, 31, enable);
  }

  void send_dlm() {
    set_reg(RORC_REG_DLM_CFG, 1);
  }

  boost::posix_time::ptime get_build_date() {
    time_t time = 
      static_cast<time_t>(get_reg(RORC_REG_BUILD_DATE_L)
                          | (static_cast<uint64_t>(get_reg(RORC_REG_BUILD_DATE_H))<<32));
    boost::posix_time::ptime t = boost::posix_time::from_time_t(time);
    return t;
  }
 
  uint16_t get_hw_ver() {
    uint16_t ver = static_cast<uint16_t>(get_reg(0) >> 16); // RORC_REG_HARDWARE_INFO
    return ver;
  }

  struct build_info {
    boost::posix_time::ptime date;
    uint32_t rev[5];
    uint16_t hw_ver;
    bool clean;
  };

  // TODO include get_build_date(), don't double reg acess
  struct build_info get_build_info() {
    build_info info;

    info.date = get_build_date();
    // TODO could be a get_mem() and in a seperate function
    info.rev[0] = get_reg(RORC_REG_BUILD_REV_0);
    info.rev[1] = get_reg(RORC_REG_BUILD_REV_1);
    info.rev[2] = get_reg(RORC_REG_BUILD_REV_2);
    info.rev[3] = get_reg(RORC_REG_BUILD_REV_3);
    info.rev[4] = get_reg(RORC_REG_BUILD_REV_4);
    info.hw_ver = get_hw_ver();
    // TODO could be get_bit()
    info.clean = (get_reg(RORC_REG_BUILD_FLAGS) & 0x1);
    return info;
  }

  std::string print_build_info() {
    build_info build = get_build_info();
    std::stringstream ss;
    ss << "Build Date:     " << build.date << std::endl
       << "Repository Revision: " << std::hex
       << build.rev[4] << build.rev[3] << build.rev[2]
       << build.rev[1] << build.rev[0] << std::endl;
    if (build.clean)
      ss << "Repository Status:   clean " << std::endl;
    else
      ss << "Repository Status:   NOT clean " << std::endl
         << "Hardware Version:    " << build.hw_ver;
    return ss.str();
  }

};

} // namespace flib
#endif // DEVICE_CTRL_CHANNEL_HPP
