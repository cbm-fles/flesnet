// C compatibility header files:
#include <cstdlib>

// C++ Standard Library header files:
#include <iostream>

// System dependent header files:
#include <sysexits.h>

// Boost Library header files:
#include <boost/program_options.hpp>

// Project header files:
#include "GitRevision.hpp"
#include "msaWriter.hpp"
#include "tsaReader.hpp"


/**
 * @file main.cpp
 * @brief Main function of the tsa2msa tool
 */

/**
 * @mainpage The tsa2msa Tool Documentation
 * @section intro_sec Introduction
 * The tsa2msa tool is a command line utility designed to convert `.tsa`
 * files to `.msa` files. Its primary purpose is to facilitate the
 * creation of golden tests for the FLESnet application by converting
 * output data from past runs that processed real experimental data.
 */

/**
 * @brief Main function
 *
 * The main function of the tsa2msa tool. Using, Boost Program Options
 * library, it parses the command line arguments and processes them
 * accordingly. TODO: Add more details. The logic to parse the command
 * line arguments may better be moved to a separate function.
 *
 * @param argc Number of command line arguments
 * @param argv Command line arguments
 * @return Exit code
 */
auto main(int argc, char* argv[]) -> int {
  // The program description below uses formatting mechanism as
  // outlined in the Boost Program Options documentation. Apart from
  // the formatting, it is intended to match the introductory
  // paragraph on the main page of the Doxygen documentation for the
  // tsa2msa tool. Any derivations should be considered as errors and
  // reported as a bugs.
  const std::string program_description =
      "tsa2msa - convert `.tsa` files to `.msa` files\n"
      "\n"
      "    Usage:\ttsa2msa [options] input1 [input2 ...]\n"
      "\n"
      "  The tsa2msa tool is a command line utility designed to\n"
      "  convert `.tsa` files to `.msa` files. Its primary purpose\n"
      "  is to facilitate the creation of golden tests for the\n"
      "  FLESnet application by converting output data from past\n"
      "  runs that processed real experimental data.\n"
      "\n"
      "  See the Doxygen documentation for the tsa2msa tool for\n"
      "  more information.\n";

  boost::program_options::options_description generic("Generic options");

  // The number of boolean switches is used to determine the number of
  // options that are passed on the command line. This number needs to
  // be manually updated whenever such a switch is added or removed.
  //
  // Note: Use alphabetical order for the switches to make it easier to
  // maintain the code.
  unsigned int nBooleanSwitches =
      4 + msaWriterNumberOfExclusiveBooleanSwitches();
  bool beQuiet = false;
  bool beVerbose = false;
  bool showHelp = false;
  bool showVersion = false;
  unsigned int nWithDefault = 1;

  generic.add_options()("quiet,q",
                        boost::program_options::bool_switch(&beQuiet),
                        "suppress all output")(
      "verbose,v", boost::program_options::bool_switch(&beVerbose),
      "enable verbose output")("help,h",
                               boost::program_options::bool_switch(&showHelp),
                               "produce help message")(
      "version,V", boost::program_options::bool_switch(&showVersion),
      "produce version message");

  msaWriterOptions msaWriterOptions = defaultMsaWriterOptions();
  boost::program_options::options_description msaWriterOptionsDescription =
      getMsaWriterOptionsDescription(msaWriterOptions, /* hidden = */ false);
  generic.add(msaWriterOptionsDescription);

  boost::program_options::options_description hidden("Hidden options");
  tsaReaderOptions tsaReaderOptions;
  boost::program_options::options_description tsaReaderOptionsDescription =
      getTsaReaderOptionsDescription(tsaReaderOptions, /* hidden = */ true);
  hidden.add(tsaReaderOptionsDescription);

  boost::program_options::positional_options_description
      positional_command_line_arguments;
  // Specify that all positional arguments are input files:
  positional_command_line_arguments.add("input", -1);

  boost::program_options::options_description command_line_options(
      program_description + "\n" + "Command line options");
  command_line_options.add(generic).add(hidden);

  boost::program_options::options_description visible_command_line_options(
      program_description + "\n" + "Command line options");
  visible_command_line_options.add(generic);

  // Parse command line options:

  bool parsingError = false;
  std::vector<std::string> errorMessage;
  boost::program_options::variables_map vm;
  try {
    // Since we are using positional arguments, we need to use the
    // command_line_parser instead of the parse_command_line
    // function, which only supports named options.
    boost::program_options::store(
        boost::program_options::command_line_parser(argc, argv)
            .options(command_line_options)
            .positional(positional_command_line_arguments)
            .run(),
        vm);
    boost::program_options::notify(vm);
  } catch (const boost::program_options::error& e) {
    errorMessage.push_back("Error: " + std::string(e.what()));
    parsingError = true;
  }

  // Count passed options:

  // Manually check each boolean switch to count how many differ from
  // their default setting:
  unsigned int nPassedBooleanSwitches = 0;
  if (beQuiet) {
    ++nPassedBooleanSwitches;
  }
  if (beVerbose) {
    ++nPassedBooleanSwitches;
  }
  if (showHelp) {
    ++nPassedBooleanSwitches;
  }
  if (showVersion) {
    ++nPassedBooleanSwitches;
  }

  unsigned int nNonBooleanSwitchOptions =
      vm.size() - nBooleanSwitches - nWithDefault;
  unsigned int nPassedOptions =
      nPassedBooleanSwitches + nNonBooleanSwitchOptions;

  // Check for parsing errors:
  if (nPassedOptions == 0) {
    errorMessage.push_back("Error: No options provided.");
    parsingError = true;
  } else if (showHelp) {
    // If the user asks for help, then we don't need to check for
    // other parsing errors. However, prior to showing the help
    // message, we will inform the user about ignoring any other
    // options and treat this as a parsing error. In contrast to
    // all other parsing errors, the error message will be shown
    // after the help message.
    unsigned int nAllowedOptions = beVerbose ? 2 : 1;
    if (nPassedOptions > nAllowedOptions) {
      parsingError = true;
    }
  } else if (showVersion) {
    if (nPassedOptions > 1) {
      errorMessage.push_back("Error: --version option cannot be combined with"
                             " other options.");
      parsingError = true;
    }
  } else if (vm.count("input") == 0) {
    errorMessage.push_back("Error: No input file provided.");
    parsingError = true;
  }

  if (parsingError) {
    for (const auto& msg : errorMessage) {
      std::cerr << msg << std::endl;
    }

    if (!showHelp) {
      // Otherwise, the user is expecting a help message, anyway.
      // So, we don't need to inform them about our decision to
      // show them usage information without having been asked.
      std::cerr << "Errors occurred: Printing usage." << std::endl << std::endl;
    }

    if (beVerbose) {
      std::cerr << command_line_options << std::endl;
    } else {
      std::cerr << visible_command_line_options << std::endl;
    }

    if (showHelp) {
      // There was a parsing error, which means that additional
      // options were provided.
      std::cerr << "Error: Ignoring any other options." << std::endl;
    }

    return EX_USAGE;
  }

  if (showHelp) {
    if (beVerbose) {
      std::cout << command_line_options << std::endl;
    } else {
      std::cout << visible_command_line_options << std::endl;
    }
    return EXIT_SUCCESS;
  } else if (showVersion) {
    std::cout << "tsa2msa version pre-alpha" << std::endl;
    std::cout << "  Project version: " << g_PROJECT_VERSION_GIT << std::endl;
    std::cout << "  Git revision: " << g_GIT_REVISION << std::endl;
    return EXIT_SUCCESS;
  }

  tsaReader tsaReader(tsaReaderOptions);

  return EXIT_SUCCESS;
}
