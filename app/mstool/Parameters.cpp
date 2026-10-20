// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Parameters.hpp"
#include "CommonOptions.hpp"
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

  size_t input_sources =
      vm.count("pattern-generator") + vm.count("input-archive");
  if (input_sources == 0) {
    throw ParametersException("no input source specified");
  }
  if (input_sources > 1) {
    throw ParametersException("more than one input source specified");
  }
}
