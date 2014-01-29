#ifndef FLIB_DEVICE_HPP
#define FLIB_DEVICE_HPP

#include <iostream>
#include <iomanip>
#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include "boost/date_time/posix_time/posix_time.hpp" //include all types plus i/o

#include "flib_link.hpp"

namespace flib {

  constexpr std::array<uint16_t, 2> hw_ver_table = {{ 1, 2}};

class flib_device {

private:

  std::unique_ptr<rorcfs_device> _dev;
  std::unique_ptr<rorcfs_bar> _bar;

  uint8_t _get_num_hw_links() {
    uint8_t n = (_bar->get(RORC_REG_N_CHANNELS) & 0xFF);
    return n;
  }

  bool _check_magic_number() {
    bool match = false;
    if (((uint16_t)(_bar->get(0) & 0xFFFF)) == 0x4844) { // RORC_REG_HARDWARE_INFO
      match = true;
    }
    return match;
  }

  bool _check_hw_ver() {
    uint16_t hw_ver =  (uint16_t)(_bar->get(0) >> 16); // RORC_REG_HARDWARE_INFO;
    bool match = false;
    // check if version of hardware is part of suported versions
    for (auto it = hw_ver_table.begin(); it != hw_ver_table.end() && match == false; ++it) {
      if (hw_ver == *it) {
        match = true;
      }
    }
    // check if version of hardware matches exactly version of header
    if (hw_ver != RORC_C_HARDWARE_VERSION) {
      match = false;
    }
    return match;
  }

public:
  std::vector<std::unique_ptr<flib_link> > link;

  // default constructor
  flib_device(int device_nr) {
    // init device class
    _dev = std::unique_ptr<rorcfs_device>(new rorcfs_device());
    if (_dev->init(device_nr) == -1) {
      throw RorcfsException("Failed to initialize device");
    }
    // bind to BAR1
    _bar = std::unique_ptr<rorcfs_bar>(new rorcfs_bar(_dev.get(), 1));
    if ( _bar->init() == -1 ) {
      throw RorcfsException("BAR1 init failed");
    }
    // enforce correct hw version
    if (!_check_hw_ver() | !_check_magic_number()) {
      throw FlibException("Unsupported hardware version");
    }
    // create link objects
    uint8_t num_links = _get_num_hw_links();
    for (size_t i=0; i<num_links; i++) {
      link.push_back(std::unique_ptr<flib_link>(new flib_link(i, _dev.get(), _bar.get())));
    }
  }
  
  // destructor
  ~flib_device() { }
  
  bool check_hw_ver() {
    return _check_hw_ver();
  }
  
  size_t get_num_links() {
    return link.size();
  }

  flib_link& get_link(size_t n) {
    return *link.at(n);
  }

  std::vector<flib_link*> get_links() {
    std::vector<flib_link*> links;
    for (auto& l : link) {
      links.push_back(l.get());
    }
    return links;
  }

  rorcfs_bar* get_bar_ptr() {
    return _bar.get();
  }
  
};
} // namespace flib
#endif // FLIB_DEVICE_HPP
