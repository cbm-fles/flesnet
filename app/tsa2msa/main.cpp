// C compatibility header files:
#include <cstdlib>

// C++ Standard Library header files:
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

// System dependent header files:
#include <sysexits.h>

// Boost Library header files:
#include <boost/program_options.hpp>

// Project header files:
#include "GitRevision.hpp"
#include "commandLineParser.hpp"
#include "msaWriter.hpp"
#include "options.hpp"
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
 * The `tsa2msa` tool is a command line utility designed to convert `.tsa`
 * files to `.msa` files. Its primary purpose is to facilitate the
 * creation of golden tests for the Flesnet application by converting
 * output data from past runs that processed real experimental data.
 *
 * @section motivation_sec Motivation
 * Experiments to develop and test CBM code are expensive
 * and time consuming. The distributed timeslice building layer
 * Flesnet is only one of many components that need to be tested, but is
 * a single point of failure for the entire experiment. Therefore,
 * testing (possibly experimental) changes and improvements to Flesnet
 * during experiments of the CBM collaboration is a delicate task.
 *
 * It is possible to test Flesnet with data from pattern generators in
 * software or from the CRI-Board hardware. However, before deploying
 * Flesnet in experiments of the CBM collaboration, it is desirable to
 * safely test it against real experimental without risking valuable
 * resources for testing other components of CBM and their interaction.
 * Furthermore, testing how Flesnet will receive data in production is
 * not possible with the pattern generator software, and the CRI-Board
 * hardware is not always available.
 *
 * From previous experiments, data is available in form of timeslice
 * archives (`.tsa` files). The `tsa2msa` tool is designed to convert
 * these `.tsa` files to microslice archives (`.msa` files). This allows
 * for a replay of the experiment data in Flesnet using the `mstool`,
 * which emulates how the `cri-server` and the CRI-boards provide data
 * in production. (However, see \subpage future_sec for how this is going to
 * change.)
 *
 * @section design_sec Design
 *
 * In contrast to Flesnet library code which is designed to be used in
 * experiments under real-time requirements, `tsa2msa` is focused on
 * file based processing and validation of data. Furthermore it serves
 * as an exploration of the Flesnet library, its capabilities and
 * current limitations. Some of the code in `tsa2msa` may later be moved
 * to the Flesnet library, but this is not the primary goal of the tool.
 *
 * The current implementation of `tsa2msa` is sequential and simple.
 * It is split into a tsaReader and a msaWriter class, and the main
 * while-read-write loop in the main function is quite simple.
 * Deliberately, changes to the Flesnet library are avoided for now.
 * Later, the tool may be extended to process data with a smaller memory
 * footprint (see \ref data_size_sec).
 *
 * @page future_sec Future Challanges
 * @section data_size_sec Data Size
 * The size of experimental data is large and the conversion of `.tsa`
 * to `.msa` files is a time and memory consuming task. While processing
 * time is not a critical issue, the memory consumption may be.
 * The current implementation of `tsa2msa` is sequential and simple,
 * using \f( \mathcal{O}(\mathit{nTimesliceArchive} \cdot
 * \mathit{MaxTimesliceSize}) \f) memory. Soon this
 * will possibly be a problem and the tool needs to be adapted to
 * process the data in smaller chunks. However, there is challenges with
 * the fact that the boost::serialization library does not provide a
 * straightforward way to "peek" into archives to determine whether it
 * contains the chronologically next timeslice.
 *
 * This issue can be overcome by either reading the entire archives once
 * to build an index and then read the data from the archives in the
 * correct order. A different approach is to attempt to copy the
 * filestream underlying the boost::archive, which may be the
 * (undocumented but) intended way to achieve "peeking" into
 * boost::archives. The former approach is likely simpler to implement,
 * but likely less time efficient. The latter approach is likely more
 * efficient, but may be more complex to implement since, currently, the
 * Flesnet library codes does not provide access to the filestreams
 * underlying the boost::archives.
 *
 * @section data_input_future_sec Changes in Data Input
 * The design and responsibilities of the `cri-server` which organizes
 * the data flow from the CRI-Board to data consumers such as Flesnet
 * are under development. The planned changes will likely make the
 * `cri-server` build sub-timeslices and `mstool` is going to loose its
 * capability to accurately emulate the data flow in production.
 *
 * Possible solutions to this problem are:
 * -# Extending `mstool` to emulate the new data flow.
 * -# Building a new tool based on `mstool` that can emulate the new
 *  data flow.
 * -# Extending `cri-server` by functionality to read either `.tsa` or
 *  `.msa` files and provide the data to Flesnet.
 *
 * Which of these solutions is going to be implemented is not yet
 * decided and each has its own drawbacks and advantages. By the very
 * idea `mstool` is designed to only work with microslice archives,
 * hence building a new tool seems more natural. However, this comes at
 * the cost of maintaining a further tool. Extending `cri-server` is
 * possibly more efficient, but so far in the development of the
 * `cri-tool` it was deliberately avoided to include functionality to
 * write data as its only purpose is to organize the communication with
 * the CRI-boards that provide the data themselves.
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
    "  Flesnet application by converting output data from past\n"
    "  runs that processed real experimental data.\n"
    "\n"
    "  See the Doxygen documentation for the tsa2msa tool for\n"
    "  more information.\n";

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

/*
 * @brief Check for errors that can be detected by the number of passed
 * options.
 *
 * @details The commandLineParser is the only class that has access to
 * this information and there is no need to pollute the options class
 * with this information (which would be equally ugly). Hence, it does
 * make sense to have all checks that involve the number of passed
 * options in a separate function.
 */
bool NumParsedOptionsAreValid(unsigned int nParsedOptions,
                              const options& opts,
                              std::vector<std::string>& errorMessage) {
  bool numOptionsError = false;
  if (nParsedOptions == 0) {
    errorMessage.push_back("Error: No options provided.");
    numOptionsError = true;
  } else if (opts.generic.showHelp) {
    // If the user asks for help, then we don't need to check for
    // other parsing errors. However, prior to showing the help
    // message, we will inform the user about ignoring any other
    // options and treat this as a parsing error. In contrast to
    // all other parsing errors, the error message will be shown
    // after the help message.
    unsigned int nAllowedOptions = opts.generic.beVerbose ? 2 : 1;
    if (nParsedOptions > nAllowedOptions) {
      if (!(opts.generic.beVerbose && nParsedOptions == 2)) {
        errorMessage.push_back("Error: --help option cannot be combined with"
                               " other options (than --verbose).");
        numOptionsError = true;
      }
    }
  } else if (opts.generic.showVersion) {
    if (nParsedOptions > 1) {
      errorMessage.push_back("Error: --version option cannot be combined with"
                             " other options.");
      numOptionsError = true;
    }
  }
  return !numOptionsError;
}

bool optionsAreValid(const options& opts,
                     const unsigned int nParsedOptions,
                     std::vector<std::string>& errorMessage) {

  if (!NumParsedOptionsAreValid(nParsedOptions, opts, errorMessage)) {
    return false;
  } else if (!opts.areValid(errorMessage)) {
    return false;
  }
  return true;
}

/**
 * @brief Handle parsing errors
 *
 * @details This function prints the error messages and usage
 * information to the standard error stream. The information is printed
 * in a way that is consistent with whether the user asked for help
 * and/or verbose output.
 */
void handleErrors(const std::string& helpMessage,
                  const std::vector<std::string>& errorMessage,
                  bool showHelp) {
  for (const auto& msg : errorMessage) {
    std::cerr << msg << std::endl;
  }

  if (!showHelp) {
    // Otherwise, the user is expecting a help message, anyway.
    // So, we don't need to inform them about our decision to
    // show them usage information without having been asked.
    std::cerr << "Errors occurred: Printing usage." << std::endl << std::endl;
  }

  std::cerr << helpMessage;

  if (showHelp) {
    // There was a parsing error, which means that additional
    // options were provided.
    std::cerr << "Error: Ignoring any other options." << std::endl;
  }
}

/*
 * @brief Make minor adjustments to the options based on the user input.
 *
 * @details This function is called after the command line arguments
 * have been parsed and validated. It makes minor adjustments to the
 * options that do not fit elsewhere, because of interdependencies
 * between sub-options structures within the options structure.
 */
void sanitizeOptions(options& options) {
  if (options.msaWriter.prefix.size() == 0) {
    std::string path_prefix = compute_common_prefix(options.tsaReader.input);

    // We only need the file name, not the entire path. This is because
    // the user likely wants to store the output files in the current
    // directory and not in the directory of the input files, which
    // likely would come as a surprise to the user.
    std::string prefix = std::filesystem::path(path_prefix).filename().string();

    // Truncate the prefix to only contain the part up to (and
    // excluding) the first `%` in order to avoid collisions with the
    // `%n` format specifier for archive sequences.
    size_t pos = prefix.find('%');
    if (pos != std::string::npos) {
      prefix = prefix.substr(0, pos);
    }

    if (prefix.size() == 0) {
      // No common prefix found, set arbitrary prefix:
      prefix = "some_prefix";
    } else {
      options.msaWriter.prefix = prefix;
    }
  }

  // \todo This msaWriter.useSequence() warning should be moved to the
  // validation of the options.
  if (options.msaWriter.useSequence()) {
    std::cerr << "Warning: Currently, the OutputArchiveSequence"
                 " classes do not properly handle the limits (at least not the"
                 " maxBytesPerArchive limit; limits may be exceeded by the"
                 " size of a micro slice.)"
              << std::endl;
  }
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

  options options(program_description);
  commandLineParser parser(options);

  // Obtain user input and validate it:
  bool error = false;
  std::vector<std::string> errorMessage;
  if (!parser.parse(argc, argv, errorMessage)) {
    errorMessage.push_back("Error: Parsing command line arguments failed.");
    error = true;
  } else {
    unsigned int nParsedOptions = parser.numParsedOptions();
    if (!optionsAreValid(options, nParsedOptions, errorMessage)) {
      errorMessage.push_back("Error: Invalid options.");
      error = true;
    }
  }

  // Handle non-default paths of execution:
  if (error) {
    handleErrors(parser.getHelpMessage(), errorMessage,
                 options.generic.showHelp);
    return EX_USAGE;
  } else if (options.generic.showHelp) {
    std::cout << parser.getHelpMessage();
    return EXIT_SUCCESS;
  } else if (options.generic.showVersion) {
    show_version();
    return EXIT_SUCCESS;
  }

  // Sanitize the options and initialize the tsaReader and msaWriter
  sanitizeOptions(options);
  tsaReader tsaReader(options.tsaReader);
  msaWriter msaWriter(options.msaWriter);

  // Read the timeslices from the input files and write them to the output
  std::unique_ptr<fles::Timeslice> timeslice;
  while ((timeslice = tsaReader.read()) != nullptr) {
    msaWriter.write(std::move(timeslice));
  }

  return EXIT_SUCCESS;
}
