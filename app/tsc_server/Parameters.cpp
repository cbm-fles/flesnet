#include "Parameters.hpp"

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
  config_add("pci-addr,i", po::value<pci_addr>(),
             "PCI BDF address of target CRI in BB:DD.F format");
  config_add("shm,o", po::value<std::string>(&_shm_id)->default_value("cri_0"),
             "name of the shared memory to be used");
  config_add("data-buffer-size-exp",
             po::value<size_t>(&_data_buffer_size_exp)->default_value(27),
             "exp. size of the data buffer in bytes");
  config_add("desc-buffer-size-exp",
             po::value<size_t>(&_desc_buffer_size_exp)->default_value(19),
             "exp. size of the descriptor buffer (number of entries)");
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

  L_(info) << "Shared memory file: " << _shm_id;
  L_(info) << print_buffer_info();
}
