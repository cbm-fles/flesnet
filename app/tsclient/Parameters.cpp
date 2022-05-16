// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Parameters.hpp"
#include "log.hpp"
#include <boost/program_options.hpp>
#include <iostream>

namespace po = boost::program_options;

void Parameters::parse_options(int argc, char* argv[]) {
  unsigned log_level = 2;
  unsigned log_syslog = 2;
  std::string log_file;

  po::options_description desc("Allowed options");
  auto desc_add = desc.add_options();
  desc_add("version,V", "print version string");
  desc_add("help,h", "produce help message");
  desc_add("log-level,l",
           po::value<unsigned>(&log_level)
               ->default_value(log_level)
               ->value_name("<n>"),
           "set the file log level (all:0)");
  desc_add("log-file,L", po::value<std::string>(&log_file),
           "name of target log file");
  desc_add("log-syslog",
           po::value<unsigned>(&log_syslog)
               ->implicit_value(log_syslog)
               ->value_name("<n>"),
           "enable logging to syslog at given log level");
  desc_add("client-index,c", po::value<int32_t>(&client_index_),
           "index of this executable in the list of processor tasks");
  desc_add("analyze-pattern,a",
           po::value<bool>(&analyze_)->implicit_value(true),
           "enable/disable pattern check");
  desc_add("monitor,m",
           po::value<std::string>(&monitor_uri_)
               ->implicit_value("http://login:8086/"),
           "publish tsclient status to InfluxDB");
  desc_add("benchmark,b", po::value<bool>(&benchmark_)->implicit_value(true),
           "run benchmark test only");
  desc_add("verbose,v", po::value<size_t>(&verbosity_), "set output verbosity");
  desc_add("histograms", po::value<bool>(&histograms_)->implicit_value(true),
           "enable microslice histogram data output");
  desc_add("input-uri,i", po::value<std::string>(&input_uri_),
           "uri of a timeslice source");
  desc_add("output-archive,o", po::value<std::string>(&output_archive_),
           "name of an output file archive to write");
  desc_add("output-archive-items", po::value<size_t>(&output_archive_items_),
           "limit number of timeslices per file to given number, create "
           "sequence of output archive files (use placeholder %n in "
           "output-archive parameter)");
  desc_add("output-archive-bytes", po::value<size_t>(&output_archive_bytes_),
           "limit number of bytes per file to given number, create "
           "sequence of output archive files (use placeholder %n in "
           "output-archive parameter)");
  desc_add(
      "publish,P",
      po::value<std::string>(&publish_address_)->implicit_value("tcp://*:5556"),
      "enable timeslice publisher on given address");
  desc_add("publish-hwm", po::value<uint32_t>(&publish_hwm_),
           "High-water mark for the publisher, in TS, TS drop happens if more "
           "buffered (default: 1)");
  desc_add("maximum-number,n", po::value<uint64_t>(&maximum_number_),
           "set the maximum number of timeslices to process (default: "
           "unlimited)");
  desc_add("rate-limit", po::value<double>(&rate_limit_),
           "limit the item rate to given frequency (in Hz)");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help") != 0u) {
    std::cout << desc << std::endl;
    exit(EXIT_SUCCESS);
  }

  if (vm.count("version") != 0u) {
    std::cout << "tsclient, version 0.0" << std::endl;
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

  size_t input_sources = vm.count("input-uri");
  if (input_sources == 0 && !benchmark_) {
    throw ParametersException("no input source specified");
  }
  if (input_sources > 1) {
    throw ParametersException("more than one input source specified");
  }
}
