/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */

#ifndef FLIB_DEVICE_HPP
#define FLIB_DEVICE_HPP

#include "boost/date_time/posix_time/posix_time.hpp"

//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wold-style-cast"

namespace pda {
class device;
class pci_bar;
}

using namespace pda;

namespace flib {
class flib_device;
class flib_link;

class register_file_bar;

struct build_info_t {
  boost::posix_time::ptime date;
  uint32_t rev[5];
  std::string host;
  std::string user;
  uint16_t hw_ver;
  bool clean;
  uint8_t repo;
};

constexpr std::array<uint16_t, 1> hw_ver_table = {{20}};

class flib_device {

public:
  flib_device(int device_nr);
  ~flib_device(){};

  bool check_hw_ver();
  void enable_mc_cnt(bool enable);
  void set_mc_time(uint32_t time);
  uint8_t number_of_hw_links();

  uint16_t hardware_version();
  boost::posix_time::ptime build_date();
  std::string build_host();
  std::string build_user();
  struct build_info_t build_info();
  std::string print_build_info();
  std::string print_devinfo();

  size_t number_of_links();
  std::vector<flib_link*> links();
  flib_link& link(size_t n);
  register_file_bar* rf() const;

private:

  /** Member variables */
  std::unique_ptr<device> m_device;
  std::unique_ptr<pci_bar> m_bar;
  std::unique_ptr<register_file_bar> m_register_file;
  std::vector<std::unique_ptr<flib_link>> m_link;

  bool check_magic_number();
};

} /** namespace flib */

// #pragma GCC diagnostic pop

#endif // FLIB_DEVICE_HPP
