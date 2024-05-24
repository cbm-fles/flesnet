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
                  ->implicit_value(log_syslog)
                  ->value_name("<n>"),
              "enable logging to syslog at given log level");



  po::options_description archive_validation("Archive validation options");
  auto validation_add = archive_validation.add_options();
  validation_add("timeslice-size", po::value<uint64_t>(&timeslice_size),
              "Used for archive validation. Set the exepected timeslice size.");
  validation_add("timeslice-cnt", po::value<uint64_t>(&timeslice_cnt),
              "Used for archive validation. Amount of expected timeslices in the timeslice archive.");
  validation_add("overlap", po::value<uint64_t>(&overlap)->default_value(overlap),
              "Used for archive validation. Timeslice overlap size.");
  validation_add("input-archives", po::value<std::vector<std::string>>(&input_archives_)->multitoken(),
            "paths to the input archives to use for validation");
  validation_add("output-archives", po::value<std::vector<std::string>>(&output_archives_)->multitoken(),
            "paths to the output archives to use for validation");
  po::options_description desc;
  desc.add(archive_validation);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help") != 0u) {
    std::cout << "mstool, git revision " << g_GIT_REVISION << std::endl;
    std::cout << desc << std::endl;
    exit(EXIT_SUCCESS);
  }

  validate_ = vm.count("input-archives") + vm.count("output-archives") > 0;
  
  bool analyze_parameter_set = true;
  if (vm.count("timeslice-cnt") == 0) {
    L_(fatal) << "'timeslice-cnt' option not set";
    analyze_parameter_set = false;
  }
  
  if (vm.count("timeslice-size") == 0) {
    L_(fatal) << "'timeslice-size' option not set";
    analyze_parameter_set = false;
  }

  if (!analyze_parameter_set) {
    throw ParametersException("Not all necessary parameter for archive analyzing set.");
  }
}
