// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Pierre-Alain Loizeau <p.-a.loizeau@gsi.de>

#include "Parameters.hpp"
#include "GitRevision.hpp"
#include "log.hpp"
#include <boost/program_options.hpp>
#include <iostream>

namespace po = boost::program_options;

void Parameters::parse_options(int argc, char* argv[]) {
  unsigned log_level = 2;
  std::string log_file;

  po::options_description general("General options");
  auto general_add = general.add_options();
  general_add("version,V", "print version string");
  general_add("help,h", "produce help message");
  general_add("log-level,l", po::value<unsigned>(&log_level),
              "set the log level (default:2, all:0)");
  general_add("log-file,L", po::value<std::string>(&log_file),
              "name of target log file");
  general_add("maximum-number,n", po::value<uint64_t>(&maximum_number),
              "set the maximum number of microslices to process (default: "
              "unlimited)");

  po::options_description source("Source options");
  auto source_add = source.add_options();
  source_add("shm-channel,c", po::value<size_t>(&shm_channel),
             "use given shared memory channel as data source");
  source_add("input-shm,I", po::value<std::string>(&input_shm),
             "name of a shared memory to use as data source");
  source_add("input-archive,i", po::value<std::string>(&input_archive),
             "name of an input file archive to read");

  po::options_description opmode("Operation mode options");
  auto opmode_add = opmode.add_options();
  opmode_add("debugger,d", po::value<bool>(&debugger)->implicit_value(true),
             "enable/disable debugger (raw message unpacking and printout)");
  opmode_add("sorter,s", po::value<bool>(&sorter)->implicit_value(true),
             "enable/disable sorting by epoch in epoch buffer for sink(s) "
             "(build output ms from epochs)");
  opmode_add("ep_in_ms,N", po::value<uint32_t>(&epoch_per_ms),
             "Number of epochs stored in each output MS in case sorter is used "
             "(default:1)");
  opmode_add("sortmesg", po::value<bool>(&sortmesg)->implicit_value(true),
             "enable/disable message sorting inside epoch buffer before "
             "creation of new ms");

  po::options_description sink("Sink options");
  auto sink_add = sink.add_options();
  sink_add("dump_verbosity,v", po::value<size_t>(&dump_verbosity),
           "set output debug dump verbosity");
  sink_add("output-shm,O", po::value<std::string>(&output_shm),
           "name of a shared memory to write to");
  sink_add("output-archive,o", po::value<std::string>(&output_archive),
           "name of an output file archive to write");

  po::options_description desc;
  desc.add(general).add(source).add(opmode).add(sink);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help") != 0u) {
    std::cout << "ngdpbtool, git revision " << g_GIT_REVISION << std::endl;
    std::cout << desc << std::endl;
    exit(EXIT_SUCCESS);
  }

  if (vm.count("version") != 0u) {
    std::cout << "ngdpbtool, git revision " << g_GIT_REVISION << std::endl;
    exit(EXIT_SUCCESS);
  }

  logging::add_console(static_cast<severity_level>(log_level));
  if (vm.count("log-file") != 0u) {
    L_(info) << "Logging output to " << log_file;
    logging::add_file(log_file, static_cast<severity_level>(log_level));
  }

  size_t input_sources = vm.count("input-archive") + vm.count("input-shm");
  if (input_sources == 0) {
    throw ParametersException("no input source specified");
  }
  if (input_sources > 1) {
    throw ParametersException("more than one input source specified");
  }
}
