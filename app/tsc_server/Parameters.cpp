// Copyright 2025 Dirk Hutter, Jan de Cuveland

#include "Parameters.hpp"
#include "MicrosliceDescriptor.hpp"
#include "Utility.hpp"
#include "log.hpp"
#include <boost/program_options.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>

namespace po = boost::program_options;

// Overload validate for PCI BDF address
void validate(boost::any& v,
              const std::vector<std::string>& values,
              pci_addr* /*unused*/,
              int /*unused*/) {
  // PCI BDF address is BB:DD.F
  static std::regex r(
      "([[:xdigit:]][[:xdigit:]]):([[:xdigit:]][[:xdigit:]]).([[:xdigit:]])");

  // Make sure no previous assignment to 'a' was made.
  po::validators::check_first_occurrence(v);
  // Extract the first string from 'values'. If there is more than
  // one string, it's an error, and exception will be thrown.
  const std::string& s = po::validators::get_single_string(values);

  // Do regex match and convert the interesting part.
  std::smatch match;
  if (std::regex_match(s, match, r)) {
    v = boost::any(pci_addr(std::stoul(match[1], nullptr, 16),
                            std::stoul(match[2], nullptr, 16),
                            std::stoul(match[3], nullptr, 16)));
  } else {
    throw po::validation_error(po::validation_error::invalid_option_value);
  }
}

std::istream& operator>>(std::istream& in, Nanoseconds& time) {
  std::string token;
  in >> token;

  // Parse the time with a suffix
  std::regex r("(-?[0-9]+)(ns|us|µs|ms|s)");
  std::smatch match;

  if (std::regex_match(token, match, r)) {
    long long value = std::stoll(match[1]);
    if (match[2] == "ns") {
      time = Nanoseconds(std::chrono::nanoseconds(value));
    } else if (match[2] == "us" || match[2] == "µs") {
      time = Nanoseconds(std::chrono::microseconds(value));
    } else if (match[2] == "ms") {
      time = Nanoseconds(std::chrono::milliseconds(value));
    } else if (match[2] == "s") {
      time = Nanoseconds(std::chrono::seconds(value));
    } else {
      throw po::invalid_option_value(token);
    }
  } else {
    throw po::invalid_option_value(token);
  }
  return in;
}

std::ostream& operator<<(std::ostream& out, const Nanoseconds& time) {
  out << time.to_str();
  return out;
}

std::string Nanoseconds::to_str() const {
  std::stringstream out;
  // Convert the time to a human-readable format
  if (count() % UINT64_C(1000000000) == 0) {
    out << count() / UINT64_C(1000000000) << "s";
  } else if (count() % UINT64_C(1000000) == 0) {
    out << count() / UINT64_C(1000000) << "ms";
  } else if (count() % UINT64_C(1000) == 0) {
    out << count() / UINT64_C(1000) << "us";
  } else {
    out << count() << "ns";
  }
  return out.str();
}

void Parameters::parse_options(int argc, char* argv[]) {

  std::string config_file;
  unsigned log_level = 2;
  unsigned log_syslog = 2;
  std::string log_file;

  po::options_description generic("Generic options");
  auto generic_add = generic.add_options();
  generic_add("help,h", "produce help message");
  generic_add(
      "config-file,c",
      po::value<std::string>(&config_file)->default_value("tsc_server.cfg"),
      "name of a configuration file");

  po::options_description config("Configuration (tsc_server.cfg or cmd line)");
  auto config_add = config.add_options();
  config_add("log-level,l",
             po::value<unsigned>(&log_level)
                 ->default_value(log_level)
                 ->value_name("<n>"),
             "set the file log level (all:0)");
  config_add("log-file,L", po::value<std::string>(&log_file),
             "name of target log file");
  config_add("log-syslog,S",
             po::value<unsigned>(&log_syslog)
                 ->implicit_value(log_syslog)
                 ->value_name("<n>"),
             "enable logging to syslog at given log level");
  config_add("monitor,m",
             po::value<std::string>(&_monitor_uri)
                 ->value_name("URI")
                 ->implicit_value("influx1:login:8086:flesnet_status"),
             "publish tsclient status to InfluxDB (or \"file:cout\" for "
             "console output)");
  config_add("pci-addr,i", po::value<pci_addr>(),
             "PCI BDF address of target CRI in BB:DD.F format");
  config_add("shm,o",
             po::value<std::string>(&_shm_id)->default_value("tsc_shm"),
             "name of the shared memory to be used");
  config_add("listen-port,p",
             po::value<uint16_t>(&_listen_port)->default_value(_listen_port),
             "port to listen for tsbuilder connections");
  config_add("tssched-address", po::value<std::string>(&_tssched_address),
             "address of the tssched server to connect to");
  config_add("pgen-channels,P",
             po::value<uint32_t>(&_pgen_channels)
                 ->default_value(_pgen_channels)
                 ->implicit_value(1),
             "number of additional pattern generator channels (0 to disable)");
  config_add(
      "pgen-microslice-duration",
      po::value<Nanoseconds>(&_pgen_microslice_duration)
          ->default_value(_pgen_microslice_duration),
      "duration of a pattern generator microslice (with suffix ns, us, ms, s)");
  config_add("pgen-microslice-size",
             po::value<size_t>(&_pgen_microslice_size)
                 ->default_value(_pgen_microslice_size),
             "size of a pattern generator microslice in bytes");
  config_add("pgen-flags",
             po::value<uint32_t>(&_pgen_flags)->default_value(_pgen_flags),
             "flags for pattern generator channels (0: no flags, "
             "1: generate pattern, 2: randomize sizes, "
             "3: generate pattern + randomize sizes, ...)");

  config_add("timeslice-duration",
             po::value<Nanoseconds>(&_timeslice_duration)
                 ->default_value(_timeslice_duration),
             "duration of a timeslice (with suffix ns, us, ms, s)");
  config_add("timeout",
             po::value<Nanoseconds>(&_timeout)->default_value(_timeout),
             "timeout for data reception (with suffix ns, us, ms, s)");
  config_add(
      "data-buffer-size",
      po::value<size_t>(&_data_buffer_size)->default_value(_data_buffer_size),
      "size of the data buffer in bytes");
  config_add(
      "desc-buffer-size",
      po::value<size_t>(&_desc_buffer_size)->default_value(_desc_buffer_size),
      "size of the descriptor buffer (number of entries)");
  config_add(
      "overlap-before",
      po::value<Nanoseconds>(&_overlap_before)->default_value(_overlap_before),
      "overlap before the timeslice (with suffix ns, us, ms, s)");
  config_add(
      "overlap-after",
      po::value<Nanoseconds>(&_overlap_after)->default_value(_overlap_after),
      "overlap after the timeslice (with suffix ns, us, ms, s)");

  po::options_description cmdline_options("Allowed options");
  cmdline_options.add(generic).add(config);

  po::options_description config_file_options;
  config_file_options.add(config);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
  po::notify(vm);

  std::ifstream ifs(config_file.c_str());
  if (!ifs) {
    if (config_file != "tsc_server.cfg") {
      throw ParametersException("Cannot open config file: " + config_file);
    }
  } else {
    std::cout << "Using config file: " << config_file << "\n";
    po::store(po::parse_config_file(ifs, config_file_options), vm);
    notify(vm);
  }

  if (vm.count("help") != 0u) {
    std::cout << cmdline_options << "\n";
    exit(EXIT_SUCCESS);
  }

  logging::add_console(static_cast<severity_level>(log_level));
  if (vm.count("log-file") != 0u) {
    L_(info) << "Logging output to " << log_file;
    logging::add_file(log_file, static_cast<severity_level>(log_level));
  }

  if (vm.count("log-syslog") != 0u) {
    logging::add_syslog(logging::syslog::local0,
                        static_cast<severity_level>(log_syslog));
  }

  if (vm.count("pci-addr") != 0u) {
    _device_address = vm["pci-addr"].as<pci_addr>();
    _device_autodetect = false;
    L_(debug) << "CRI address: " << std::hex << std::setw(2)
              << std::setfill('0') << static_cast<unsigned>(_device_address.bus)
              << ":" << std::setw(2) << std::setfill('0')
              << static_cast<unsigned>(_device_address.dev) << "."
              << static_cast<unsigned>(_device_address.func);
  } else {
    _device_autodetect = true;
    L_(debug) << "CRI address: autodetect";
  }

  if (timeslice_duration_ns() <= 0) {
    throw ParametersException("timeslice duration must be greater than 0");
  }
  if ((overlap_before_ns() + overlap_after_ns()) < 0) {
    throw ParametersException("negative total overlap cuts into timeslice");
  }
  if (timeout_ns() <= 0) {
    throw ParametersException("timeout must be greater than 0");
  }

  L_(info) << "Shared memory file: " << _shm_id;
  L_(info) << buffer_info();
}

[[nodiscard]] std::string Parameters::buffer_info() const {
  std::stringstream ss;
  ss << "Buffer size per channel: " << human_readable_count(_data_buffer_size)
     << " + "
     << human_readable_count(_desc_buffer_size *
                             sizeof(fles::MicrosliceDescriptor));
  return ss.str();
}
