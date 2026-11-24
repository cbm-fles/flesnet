// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Parameters.hpp"
#include "CommonOptions.hpp"
#include "OptionValues.hpp"
#include "System.hpp"
#include "log.hpp"
#include "log_program_options.hpp"
// #include <boost/program_options.hpp>
// #include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace po = boost::program_options;

void Parameters::parse_options(int argc, char* argv[]) {

  po::options_description general("General options");
  auto general_add = general.add_options();
  fles::add_general_options(general);
  logging::add_program_options(general);
  general_add("maximum-number,n", po::value<uint64_t>(&maximum_number),
              "set the maximum number of microslices to process (default: "
              "unlimited)");

  po::options_description source("Source options");
  auto source_add = source.add_options();
  source_add("pattern-generator,p", po::value<uint32_t>(&pattern_generator),
             "use pattern generator to produce timeslices");
  source_add("channel,c", po::value<size_t>(&channel_idx),
             "use given channel/component index for source/sink");
  source_add("microslice-size",
             po::value<uint32_t>(&microslice_size)
                 ->default_value(microslice_size)
                 ->value_name("N"),
             "typical content size in bytes of a pattern generator microslice");
  source_add("microslice-duration",
             po::value<fles::Nanoseconds>()->value_name("DURATION"),
             "duration of a pattern generator microslice (with suffix ns, us, "
             "ms, s); the start times of the microslices advance by it "
             "(default: the microslices are numbered instead)");
  source_add("start-time", po::value<std::string>()->value_name("TIME"),
             "start time of the first pattern generator microslice, as "
             "nanoseconds or ISO 8601 (default: 0)");
  source_add("input-archive,i", po::value<std::string>(&input_archive),
             "name of an input file archive to read");

  po::options_description sink("Sink options");
  auto sink_add = sink.add_options();
  sink_add("analyze,a", po::value<bool>(&analyze)->implicit_value(true),
           "enable/disable pattern check");
  sink_add("dump_verbosity,v", po::value<size_t>(&dump_verbosity),
           "set output debug dump verbosity");
  sink_add("output-archive,o", po::value<std::string>(&output_archive),
           "name of an output file archive to write");

  po::options_description desc;
  desc.add(general).add(source).add(sink);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  fles::handle_general_options(vm, "mstool", desc);

  logging::setup(fles::log_options(vm));

  use_pattern_generator = vm.count("pattern-generator") != 0;
  if (vm.count("microslice-duration") != 0) {
    microslice_duration =
        vm["microslice-duration"].as<fles::Nanoseconds>().count();
    if (microslice_duration <= 0) {
      throw ParametersException("microslice duration must be greater than 0");
    }
  }
  if (vm.count("start-time") != 0) {
    start_time =
        fles::system::parse_time_point_ns(vm["start-time"].as<std::string>());
  }

  size_t input_sources =
      vm.count("pattern-generator") + vm.count("input-archive");
  if (input_sources == 0) {
    throw ParametersException("no input source specified");
  }
  if (input_sources > 1) {
    throw ParametersException("more than one input source specified");
  }
}
