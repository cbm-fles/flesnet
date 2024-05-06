// C compatibility header files:
#include <cstdlib>

// C++ Standard Library header files:
#include <iostream>
#include <string>
#include <vector>

// System dependent header files:
#include <sysexits.h>

// Boost Library header files:
#include <boost/program_options.hpp>

// Project header files:
#include "GitRevision.hpp"
#include "msaWriter.hpp"
#include "tsaReader.hpp"
#include "utils.hpp"

/**
 * @file main.cpp
 * @brief Contains the main function of the tsa2msa tool as well as
 * anything related to parsing the command line arguments. Global
 * options are defined here, too, but options specific to components of
 * the tool are defined in their respective files.
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
 * @brief Program description for the command line help message
 * @details This string contains the introductory paragraph of the
 * doxygen documentation for the tsa2msa tool, but formatted according
 * to mechanism as outlined in the Boost Program Options documentation.
 * Any derivation from the original text should be considered as an
 * error and reported as a bug.
 */
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

/**
 * @brief Parse command line arguments and store them in a variables map
 * @details This function parses the command line arguments using the
 * Boost Program Options library, storing the results in a variables map
 * provided by the caller. If the boost library throws an exception, an
 * appropriate error message is pushed back to the error message vector.
 *
 * @param argc Number of command line arguments as passed to main
 * @param argv Command line arguments as passed to main to main
 * @param command_line_options Description of all command line options
 * @param positional_command_line_arguments Description of all
 * positional command line arguments
 * @param vm Variables map to store the parsed options
 * @param errorMessage Vector to store error messages
 * @return True if there was a parsing error, false otherwise
 */
bool parse_command_line(int argc, char* argv[],
                        const boost::program_options::options_description& command_line_options,
                        const boost::program_options::positional_options_description& positional_command_line_arguments,
                        boost::program_options::variables_map& vm,
                        std::vector<std::string>& errorMessage) {
  bool parsingError = false;
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
  return parsingError;
}

/**
 * @brief Check for global parsing errors
 *
 * @details Checks whether input files were provided and whether the
 * logical errors of global options are present. I.e. logical errors
 * that are not specific to any particular component of the tool.
 *
 * @note The values of the variables beVerbose, showHelp, and showVersion
 * need to be passed as arguments as they are obtained via boolean
 * switches. It is possible to recover their values from the variables
 * map, but this misses the point of using boolean switches to prevent
 * error prone manual lookups in the variables map.
 *
 * \pre The variables map should have been populated.
 * \pre The value of the boolean switches beVerbose, showHelp, and
 * showVersion should be as provided by the user on the command line.
 *
 * @param vm Variables map containing the parsed options
 * @param errorMessage Vector to store error messages
 * @param beVerbose Whether verbose output is enabled
 * @param showHelp Whether the user asked for help
 * @param showVersion Whether the user asked for the version
 * @return True if there was a parsing error, false otherwise
 */
bool check_for_global_parsing_errors(const boost::program_options::variables_map& vm,
                                     std::vector<std::string>& errorMessage,
                                     const bool& beVerbose, const bool& showHelp,
                                     const bool& showVersion) {
  bool parsingError = false;

  if (errorMessage.size() > 0) {
    // TODO: Handle this case more gracefully.
    std::cerr << "Warning: Expected that so far no error messages are present.";
  }

  // Count passed options:
  unsigned int nPassedOptions = 0;
  for (const auto& option : vm) {
    if (!option.second.defaulted()) {
      nPassedOptions++;
    } else {
      // This option is present, but only because it has a default value
      // which was used. I.e. the user did not provide this option.
    }
  }

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

  return parsingError;

}


/**
 * @brief Handle parsing errors
 *
 * @details This function prints the error messages and usage
 * information to the standard error stream. The information is printed
 * in a way that is consistent with whether the user asked for help
 * and/or verbose output.
 */
void handle_parsing_errors(const std::vector<std::string>& errorMessage,
                           const boost::program_options::options_description& command_line_options,
                           const boost::program_options::options_description& visible_command_line_options,
                           const bool& beVerbose, const bool& showHelp) {
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
}

/**
 * @brief Show help message
 *
 * @details This function prints the help message to the standard output
 * stream. The information is printed in a way that is consistent with
 * whether the user asked for verbose output.
 */
void show_help(const boost::program_options::options_description& command_line_options,
    const boost::program_options::options_description& visible_command_line_options,
    const bool& beVerbose ) {
  if (beVerbose) {
    std::cout << command_line_options << std::endl;
  } else {
    std::cout << visible_command_line_options << std::endl;
  }
}

/**
 * @brief Show version information
 *
 * @details This function prints the version information to the standard
 * output stream.
 */
void show_version() {
    std::cout << "tsa2msa version pre-alpha" << std::endl;
    std::cout << "  Project version: " << g_PROJECT_VERSION_GIT << std::endl;
    std::cout << "  Git revision: " << g_GIT_REVISION << std::endl;
}



/**
 * @brief Main function
 *
 * The main function of the tsa2msa tool. Using, Boost Program Options
 * library, it parses the command line arguments and processes them
 * accordingly.
 *
 * \todo Check that the exit codes indeed comply with the `<sysexits.h>`.
 * \todo Extract the options logic to a separate class resp. file.
 * \todo Create a `struct tsa2msaGlobalOptions` to hold all global options.
 * \todo Create an `--output-dir` option (and a `--mkdir` sub-option).
 *
 * @return Exit code according to the `<sysexits.h>` standard.
 */
auto main(int argc, char* argv[]) -> int {

  boost::program_options::options_description generic("Generic options");

  // Note: Use alphabetical order for the switches to make it easier to
  // maintain the code.
  bool beQuiet = false;
  bool beVerbose = false;
  bool showHelp = false;
  bool showVersion = false;

  // TODO: Create a class for these options:
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
  tsaReaderOptions tsaReaderOptions = defaultTsaReaderOptions();
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
  std::vector<std::string> errorMessage;
  boost::program_options::variables_map vm;
  bool parsingError = parse_command_line(argc, argv, command_line_options,
                                         positional_command_line_arguments,
                                         vm,
                                         errorMessage);

  // Check for further parsing errors:
  if (!parsingError) {
    parsingError = check_for_global_parsing_errors(vm, errorMessage,
                                                    beVerbose, showHelp,
                                                    showVersion);
  }

  if (parsingError) {
    handle_parsing_errors(errorMessage, command_line_options,
                          visible_command_line_options, beVerbose, showHelp);
    return EX_USAGE;
  }

  if (showHelp) {
    show_help(command_line_options, visible_command_line_options, beVerbose);
    return EXIT_SUCCESS;
  } else if (showVersion) {
    show_version();
    return EXIT_SUCCESS;
  }

  getTsaReaderOptions(vm, tsaReaderOptions);
  tsaReader tsaReader(tsaReaderOptions);
  msaWriter msaWriter(msaWriterOptions);

  std::string prefix = msaWriterOptions.prefix;
  if (prefix.size() == 0) {
    msaWriterOptions.prefix = compute_common_prefix(tsaReaderOptions.input);

  }

  clean_up_path(msaWriterOptions.prefix);

  std::unique_ptr<fles::Timeslice> timeslice;
  while ((timeslice = tsaReader.read()) != nullptr) {
    msaWriter.write_timeslice(std::move(timeslice));
  }

  return EXIT_SUCCESS;
}
