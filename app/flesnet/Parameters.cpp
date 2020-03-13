// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Parameters.hpp"
#include "GitRevision.hpp"
#include "MicrosliceDescriptor.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include "Utility.hpp"
#include "log.hpp"
#include <algorithm>
#include <boost/algorithm/string/join.hpp>
#include <boost/program_options.hpp>
#include <cpprest/base_uri.h>
#include <fstream>
#include <iterator>

namespace po = boost::program_options;

std::istream& operator>>(std::istream& in, Transport& transport) {
  std::string token;
  in >> token;
  std::transform(std::begin(token), std::end(token), std::begin(token),
                 [](const unsigned char i) { return tolower(i); });

  if (token == "rdma" || token == "r") {
    transport = Transport::RDMA;
  } else if (token == "libfabric" || token == "f") {
    transport = Transport::LibFabric;
  } else if (token == "zeromq" || token == "z") {
    transport = Transport::ZeroMQ;
  } else {
    throw po::invalid_option_value(token);
  }
  return in;
}

std::ostream& operator<<(std::ostream& out, const Transport& transport) {
  switch (transport) {
  case Transport::RDMA:
    out << "RDMA";
    break;
  case Transport::LibFabric:
    out << "LibFabric";
    break;
  case Transport::ZeroMQ:
    out << "ZeroMQ";
    break;
  }
  return out;
}

std::istream& operator>>(std::istream& in, InterfaceSpecification& ifspec) {
  in >> ifspec.full_uri;
  try {
    web::uri uri(ifspec.full_uri);
    ifspec.scheme = uri.scheme();
    ifspec.host = uri.host();
    ifspec.path = web::uri::split_path(uri.path());
    ifspec.param = web::uri::split_query(uri.query());
  } catch (const web::uri_exception& e) {
    throw po::invalid_option_value(ifspec.full_uri);
  }
  return in;
}

std::ostream& operator<<(std::ostream& out,
                         const InterfaceSpecification& ifspec) {
  out << ifspec.full_uri;
  return out;
}

void Parameters::parse_options(int argc, char* argv[]) {
  unsigned log_level = 2;
  unsigned log_syslog = 2;
  std::string log_file;
  std::string config_file;

  po::options_description generic("Generic options");
  auto generic_add = generic.add_options();
  generic_add("config-file,f",
              po::value<std::string>(&config_file)->value_name("<filename>"),
              "read configuration from file");
  generic_add("log-level,l",
              po::value<unsigned>(&log_level)
                  ->default_value(log_level)
                  ->value_name("<n>"),
              "set the file log level (all:0)");
  generic_add("log-file,L",
              po::value<std::string>(&log_file)->value_name("<filename>"),
              "write log output to file");
  generic_add("log-syslog,S",
              po::value<unsigned>(&log_syslog)
                  ->default_value(log_syslog)
                  ->value_name("<n>"),
              "enable logging to syslog at given log level");
  generic_add("monitor,m",
              po::value<std::string>(&monitor_uri_)
                  ->implicit_value("http://login:8086/"),
              "publish flesnet status to InfluxDB");
  generic_add("help,h", "display this help and exit");
  generic_add("version,V", "output version information and exit");

  po::options_description config("Configuration");
  auto config_add = config.add_options();
  config_add(
      "input-index,i",
      po::value<std::vector<unsigned>>()->multitoken()->value_name("<n> ..."),
      "set this application's index(es) in the list of inputs");
  config_add(
      "output-index,o",
      po::value<std::vector<unsigned>>()->multitoken()->value_name("<n> ..."),
      "set this application's index(es) in the list of outputs");
  config_add("input,I",
             po::value<std::vector<InterfaceSpecification>>()
                 ->multitoken()
                 ->value_name("scheme://host/path?param=value ..."),
             "add an input to the list of participating inputs");
  config_add("output,O",
             po::value<std::vector<InterfaceSpecification>>()
                 ->multitoken()
                 ->value_name("scheme://host/path?param=value ..."),
             "add an output to the list of compute node outputs");
  config_add("timeslice-size",
             po::value<uint32_t>(&timeslice_size_)
                 ->default_value(timeslice_size_)
                 ->value_name("<n>"),
             "set the global timeslice size in number of microslices");
  config_add("max-timeslice-number,n",
             po::value<uint32_t>(&max_timeslice_number_)->value_name("<n>"),
             "quit after processing given number of timeslices");
  config_add(
      "processor-executable,e",
      po::value<std::string>(&processor_executable_)->value_name("<string>"),
      "name of the executable acting as timeslice processor");
  config_add("processor-instances",
             po::value<uint32_t>(&processor_instances_)
                 ->default_value(processor_instances_)
                 ->value_name("<n>"),
             "number of instances of the timeslice processor executable");
  config_add("base-port",
             po::value<uint32_t>(&base_port_)
                 ->default_value(base_port_)
                 ->value_name("<n>"),
             "base IP port to use for listening");
  config_add("transport,t",
             po::value<Transport>(&transport_)
                 ->default_value(transport_)
                 ->value_name("<id>"),
             "select transport implementation; possible values "
             "(case-insensitive) are: RDMA, LibFabric, ZeroMQ");

  po::options_description cmdline_options("Allowed options");
  cmdline_options.add(generic).add(config);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
  po::notify(vm);

  if (!config_file.empty()) {
    std::ifstream ifs(config_file.c_str());
    if (!ifs) {
      throw ParametersException("cannot open config file: " + config_file);
    }
    po::store(po::parse_config_file(ifs, config), vm);
    notify(vm);
  }

  if (vm.count("help") != 0u) {
    std::cout << "flesnet, git revision " << g_GIT_REVISION << std::endl;
    std::cout << cmdline_options << std::endl;
    exit(EXIT_SUCCESS);
  }

  if (vm.count("version") != 0u) {
    std::cout << "flesnet, git revision " << g_GIT_REVISION << std::endl;
    exit(EXIT_SUCCESS);
  }

  logging::add_console(static_cast<severity_level>(log_level));
  if (vm.count("log-file") != 0u) {
    L_(info) << "logging output to " << log_file;
    if (log_level < 3) {
      log_level = 3;
      L_(info) << "increased file log level to " << log_level;
    }
    logging::add_file(log_file, static_cast<severity_level>(log_level));
  }
  if (vm.count("log-syslog") != 0u) {
    logging::add_syslog(logging::syslog::local0,
                        static_cast<severity_level>(log_syslog));
  }

  if (timeslice_size_ < 1) {
    throw ParametersException("timeslice size cannot be zero");
  }

#ifndef HAVE_RDMA
  if (transport_ == Transport::RDMA) {
    throw ParametersException("flesnet built without RDMA support");
  }
#endif
#ifndef HAVE_LIBFABRIC
  if (transport_ == Transport::LibFabric) {
    throw ParametersException("flesnet built without LIBFABRIC support");
  }
#endif

  if (vm.count("input") == 0u) {
    throw ParametersException("list of inputs is empty");
  }

  if (vm.count("output") == 0u) {
    throw ParametersException("list of outputs is empty");
  }

  inputs_ = vm["input"].as<std::vector<InterfaceSpecification>>();
  outputs_ = vm["output"].as<std::vector<InterfaceSpecification>>();

  for (auto& input : inputs_) {
    if (!web::uri::validate(input.full_uri)) {
      throw ParametersException("invalid input specification: " +
                                input.full_uri);
    }
  }

  for (auto& output : outputs_) {
    if (!web::uri::validate(output.full_uri)) {
      throw ParametersException("invalid output specification: " +
                                output.full_uri);
    }
  }

  if (vm.count("input-index") != 0u) {
    input_indexes_ = vm["input-index"].as<std::vector<unsigned>>();
  }
  if (vm.count("output-index") != 0u) {
    output_indexes_ = vm["output-index"].as<std::vector<unsigned>>();
  }

  if (inputs_.empty() && outputs_.empty()) {
    throw ParametersException("no node type specified");
  }

  for (auto input_index : input_indexes_) {
    if (input_index >= inputs_.size()) {
      std::ostringstream oss;
      oss << "input index (" << input_index << ") out of range (0.."
          << inputs_.size() - 1 << ")";
      throw ParametersException(oss.str());
    }
  }

  for (auto output_index : output_indexes_) {
    if (output_index >= outputs_.size()) {
      std::ostringstream oss;
      oss << "output index (" << output_index << ") out of range (0.."
          << outputs_.size() - 1 << ")";
      throw ParametersException(oss.str());
    }
  }

  if (!outputs_.empty() && processor_executable_.empty()) {
    throw ParametersException("processor executable not specified");
  }

  L_(debug) << "inputs (" << inputs_.size() << "):";
  for (auto input : inputs_) {
    L_(debug) << "  " << input.full_uri;
  }
  L_(debug) << "outputs (" << outputs_.size() << "):";
  for (auto output : outputs_) {
    L_(debug) << "  " << output.full_uri;
  }
  for (auto input_index : input_indexes_) {
    L_(info) << "this is input " << input_index << " (of " << inputs_.size()
             << ")";
  }
  for (auto output_index : output_indexes_) {
    L_(info) << "this is output " << output_index << " (of " << outputs_.size()
             << ")";
  }

  for (auto input_index : input_indexes_) {
    if (input_index == 0) {
      L_(info) << "timeslice size: " << timeslice_size_ << " microslices";
      L_(info) << "number of timeslices: " << max_timeslice_number_;
    }
  }
}
