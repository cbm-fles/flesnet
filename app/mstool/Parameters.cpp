// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Parameters.hpp"
#include "GitRevision.hpp"
#include "log.hpp"
#include <boost/program_options.hpp>
#include <iostream>

namespace po = boost::program_options;

void Parameters::parse_options(int argc, char* argv[]) {
  unsigned log_level = 2;
  unsigned log_syslog = 2;
  std::string log_file;

  po::options_description general("General options");
  auto general_add = general.add_options();
  general_add("version,V", "print version string");
  general_add("help,h", "produce help message");
  general_add("log-level,l",
              po::value<unsigned>(&log_level)
                  ->default_value(log_level)
                  ->value_name("<n>"),
              "set the file log level (all:0)");
  general_add("log-file,L", po::value<std::string>(&log_file),
              "name of target log file");
  general_add("log-syslog,S",
              po::value<unsigned>(&log_syslog)
                  ->default_value(log_syslog)
                  ->value_name("<n>"),
              "enable logging to syslog at given log level");
  general_add("maximum-number,n", po::value<uint64_t>(&maximum_number),
              "set the maximum number of microslices to process (default: "
              "unlimited)");
  general_add("exec,e", po::value<std::string>(&exec)->value_name("<string>"),
              "name of an executable to run after startup");

  po::options_description source("Source options");
  auto source_add = source.add_options();
  source_add("pattern-generator,p", po::value<uint32_t>(&pattern_generator),
             "use pattern generator to produce timeslices");
  source_add("channel,c", po::value<size_t>(&channel_idx),
             "use given channel/component index for source/sink");
  source_add("input-shm,I", po::value<std::string>(&input_shm),
             "name of a shared memory to use as data source");
  source_add("input-archive,i", po::value<std::string>(&input_archive),
             "name of an input file archive to read");

  po::options_description sink("Sink options");
  auto sink_add = sink.add_options();
  sink_add("analyze,a", po::value<bool>(&analyze)->implicit_value(true),
           "enable/disable pattern check");
  sink_add("dump_verbosity,v", po::value<size_t>(&dump_verbosity),
           "set output debug dump verbosity");
  sink_add("output-shm,O", po::value<std::string>(&output_shm),
           "name of a shared memory to write to");
  sink_add("output-archive,o", po::value<std::string>(&output_archive),
           "name of an output file archive to write");

  po::options_description desc;
  desc.add(general).add(source).add(sink);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help") != 0u) {
    std::cout << "mstool, git revision " << g_GIT_REVISION << std::endl;
    std::cout << desc << std::endl;
    exit(EXIT_SUCCESS);
  }

  if (vm.count("version") != 0u) {
    std::cout << "mstool, git revision " << g_GIT_REVISION << std::endl;
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

  use_pattern_generator = vm.count("pattern-generator") != 0;

  size_t input_sources = vm.count("pattern-generator") +
                         vm.count("input-archive") + vm.count("input-shm");
  if (input_sources == 0) {
    throw ParametersException("no input source specified");
  }
  if (input_sources > 1) {
    throw ParametersException("more than one input source specified");
  }
}
