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
  unsigned int nOptionsWithDefaults =
      0 + TsaReaderNumberOfOptionsWithDefaults();

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
      vm.size() - nBooleanSwitches - nOptionsWithDefaults;
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

  getTsaReaderOptions(vm, tsaReaderOptions);
  tsaReader tsaReader(tsaReaderOptions);

  // TODO: Clean up the following:
  std::string prefix = msaWriterOptions.prefix;
  if (prefix.size() == 0) {

    // Compute common prefix of input files:
    //
    // TODO: Implement the following less hacky and at some more
    // appropriate place:
    if (tsaReaderOptions.input.size() > 0) {
      prefix = tsaReaderOptions.input[0];
    } else {
      std::cerr << "Error: Unreachable code reached." << std::endl;
      return EX_SOFTWARE;
    }
    for (const auto& input : tsaReaderOptions.input) {
      for (unsigned i = 0; i < prefix.size(); ++i) {
        if (i >= input.size()) {
          prefix = prefix.substr(0, i);
          break;
        }
        if (prefix[i] != input[i]) {
          prefix = prefix.substr(0, i);
          break;
        }
      }
      if (prefix.size() == 0) {
        break;
      }
    }

    // Remove everything except for the file name:
    //
    // TODO: Use boost::filesystem::path instead of the following hacky
    // code.
    //
    // Check if linux:
#ifdef __linux__
    size_t last_slash = prefix.find_last_of('/');
    if (last_slash != std::string::npos) {
      // Add 1 to remove the slash itself:
      prefix = prefix.substr(last_slash + 1);
    }
    if (prefix.size() == 0) {
      // No common prefix found, set arbitrary prefix:
      prefix = "some_prefix";
    }

#else  // __linux__
    std::cerr << "Error: Using the common prefix of input files as the"
              << std::endl
              << "       prefix for the output files is only implemented"
              << std::endl
              << "       for linux systems. Using arbitrary prefix instead."
              << std::endl;
    prefix = "some_prefix";
#endif // __linux__
  }

  // TODO: We need an output directory option.

  std::unique_ptr<fles::Timeslice> timeslice;
  // TODO: Get rid of the unique_ptr and use emplace instead
  // TODO: Get rid of the string as a key and use some enum class or
  // tuple or something else as a key (which simultaneously could be
  // used to derive the string for the filename).
  std::map<std::string, std::unique_ptr<fles::MicrosliceOutputArchive>>
      msaFiles;

  uint64_t numTimeslices = 0;
  uint64_t numMicroslices = 0;

  // MicrosliceDescriptor fields with custom enum classes (apart from
  // fles::Subsystem, which is commented out, because it is not used in
  // the checks):
  // fles::Subsystem last_sys_id;
  fles::SubsystemFormatFLES last_sys_ver;
  fles::HeaderFormatIdentifier last_hdr_id;
  fles::HeaderFormatVersion last_hdr_ver;
  fles::MicrosliceFlags last_flags;

  // All fields of the MicrosliceDescriptor together with the comment
  // from the MicrosliceDescriptor class in the order of their
  // appearance. The ones have their own enum classes are commented out,
  // as well as the fields that currently do not need to be checked.
  // (Note that these enum classes are not used in the code of the
  // MicrosliceDescriptor class, but they are defined in the same
  // header.)
  //
  // uint8_t hdr_id;  ///< Header format identifier (0xDD)
  // uint8_t hdr_ver; ///< Header format version (0x01)
  uint16_t last_eq_id; ///< Equipment identifier
  // uint16_t flags;  ///< Status and error flags
  // uint8_t sys_id;  ///< Subsystem identifier
  // uint8_t sys_ver; ///< Subsystem format/version
  // uint64_t idx;    ///< Microslice index / start time
  // uint32_t crc;    ///< CRC-32C (Castagnoli polynomial) of data content
  // uint32_t size;   ///< Content size (bytes)
  // uint64_t offset; ///< Offset in event buffer (bytes)

  // Setting the last values to zero to avoid a false positive in the
  // compiler warning about uninitialized variables:
  last_hdr_id = static_cast<fles::HeaderFormatIdentifier>(0);
  last_hdr_ver = static_cast<fles::HeaderFormatVersion>(0);
  last_eq_id = static_cast<uint16_t>(0);
  last_flags = static_cast<fles::MicrosliceFlags>(0);
  // last_sys_id = static_cast<fles::Subsystem>(0); // Not used
  last_sys_ver = static_cast<fles::SubsystemFormatFLES>(0);

  // Statistics about their values in the order of their appearance in
  // the MicrosliceDescriptor class:

  // Change-Counters:
  uint64_t num_hdr_id_changes = 0;
  uint64_t num_hdr_ver_changes = 0;
  uint64_t num_eq_id_changes = 0;
  uint64_t num_flags_changes = 0;
  // uint64_t num_sys_id_changes = 0; // Not used
  uint64_t num_sys_ver_changes = 0;

  // Appeared values.
  //
  // A set is used, because only a few values are expected to appear.
  // The constant look-up/insertion time of unordered_sets likely is not
  // going to be faster than the logarithmic look-up time of sets for
  // such small sets, but likely the other way around. (Did not measure
  // it myself, though, see
  // https://stackoverflow.com/questions/1349734).
  std::set<fles::HeaderFormatIdentifier> hdr_id_seen;
  std::set<fles::HeaderFormatVersion> hdr_ver_seen;
  std::set<uint16_t> eq_id_seen;
  std::set<fles::MicrosliceFlags> flags_seen;
  std::set<fles::Subsystem> sys_id_seen;
  std::set<fles::SubsystemFormatFLES> sys_ver_seen;

  while ((timeslice = tsaReader.read()) != nullptr) {
    // TODO: Count sys_id changes per timeslice, check if there is new
    // or missing sys_ids.

    // Check if some values stay constant per Timeslice:
    std::set<fles::HeaderFormatIdentifier> hdr_id_seen_in_ts;
    std::set<fles::HeaderFormatVersion> hdr_ver_seen_in_ts;
    std::set<uint16_t> eq_id_seen_in_ts;
    std::set<fles::MicrosliceFlags> flags_seen_in_ts;
    std::set<fles::Subsystem> sys_id_seen_in_ts;
    std::set<fles::SubsystemFormatFLES> sys_ver_seen_in_ts;
    uint64_t numMicroslices_in_ts = 0;

    // Map to count the number of Equipment Identifiers appearing:
    std::map<uint16_t, uint64_t> eq_id_count;

    // Write the timeslice to a file:
    for (uint64_t tsc = 0; tsc < timeslice->num_components(); tsc++) {
      // TODO: Count sys_id changes per TimesliceComponent, check if
      // there is new or missing sys_ids.

      // Check if some values stay constant per TimesliceComponent:
      std::set<fles::HeaderFormatIdentifier> hdr_id_seen_in_tsc;
      std::set<fles::HeaderFormatVersion> hdr_ver_seen_in_tsc;
      std::set<uint16_t> eq_id_seen_in_tsc;
      std::set<fles::MicrosliceFlags> flags_seen_in_tsc;
      std::set<fles::Subsystem> sys_id_seen_in_tsc;
      std::set<fles::SubsystemFormatFLES> sys_ver_seen_in_tsc;
      uint64_t numMicroslices_in_tsc = 0;

      // for (uint64_t msc = 0; msc < timeslice->num_core_microslices(); msc++)
      // {
      for (uint64_t msc = 0; msc < timeslice->num_microslices(tsc); msc++) {
        std::unique_ptr<fles::MicrosliceView> ms_ptr =
            std::make_unique<fles::MicrosliceView>(
                timeslice->get_microslice(tsc, msc));
        const fles::MicrosliceDescriptor& msd = ms_ptr->desc();

        // Get values from the MicrosliceDescriptor:
        // TODO: Take into account that raw values not corresponding to
        // some named enum value should be treated as an error.
        const fles::HeaderFormatIdentifier& hdr_id =
            static_cast<fles::HeaderFormatIdentifier>(msd.hdr_id);
        const fles::HeaderFormatVersion& hdr_ver =
            static_cast<fles::HeaderFormatVersion>(msd.hdr_ver);
        const uint16_t& eq_id = msd.eq_id;
        const fles::MicrosliceFlags& flags =
            static_cast<fles::MicrosliceFlags>(msd.flags);
        const fles::Subsystem& sys_id =
            static_cast<fles::Subsystem>(msd.sys_id);
        const fles::SubsystemFormatFLES& sys_ver =
            static_cast<fles::SubsystemFormatFLES>(msd.sys_ver);

        // Insert values into the seen sets:
        hdr_id_seen.insert(hdr_id);
        hdr_ver_seen.insert(hdr_ver);
        eq_id_seen.insert(eq_id);
        flags_seen.insert(flags);
        sys_id_seen.insert(sys_id);
        sys_ver_seen.insert(sys_ver);

        // Insert values into the seen sets of the Timeslice:
        hdr_id_seen_in_ts.insert(hdr_id);
        hdr_ver_seen_in_ts.insert(hdr_ver);
        eq_id_seen_in_ts.insert(eq_id);
        flags_seen_in_ts.insert(flags);
        sys_id_seen_in_ts.insert(sys_id);
        sys_ver_seen_in_ts.insert(sys_ver);

        // Insert values into the seen sets of the TimesliceComponent:
        hdr_id_seen_in_tsc.insert(hdr_id);
        hdr_ver_seen_in_tsc.insert(hdr_ver);
        auto tsc_insertion_result_eq_id = eq_id_seen_in_tsc.insert(eq_id);
        flags_seen_in_tsc.insert(flags);
        sys_id_seen_in_tsc.insert(sys_id);
        sys_ver_seen_in_tsc.insert(sys_ver);

        if (tsc_insertion_result_eq_id.second) {
          // If the Equipment Identifier is new, insert it into the
          // count map with a count of 1. This effectively counts the
          // number of Equipment Identifiers appearing in the
          // TimesliceComponent.
          //
          // Note: According to the C++ standard, the container
          // zero-initializes the value if the key is not found (or for
          // class types, the default constructor is called). This is
          // why it is sufficient to simply increment the value here.
          eq_id_count[eq_id]++;
        }

        // Compare to last values:
        if (numMicroslices > 0) {
          if (hdr_id != last_hdr_id) {
            ++num_hdr_id_changes;
          }
          if (hdr_ver != last_hdr_ver) {
            ++num_hdr_ver_changes;
          }
          if (eq_id != last_eq_id) {
            ++num_eq_id_changes;
          }
          if (flags != last_flags) {
            ++num_flags_changes;
          }
          // if (sys_id != last_sys_id) { // Not used
          //  ++num_sys_id_changes;
          //}
          if (sys_ver != last_sys_ver) {
            ++num_sys_ver_changes;
          }
        }
        numMicroslices++;
        numMicroslices_in_ts++;
        numMicroslices_in_tsc++;

        // Set last values
        last_hdr_id = static_cast<fles::HeaderFormatIdentifier>(msd.hdr_id);
        last_hdr_ver = static_cast<fles::HeaderFormatVersion>(msd.hdr_ver);
        last_eq_id = msd.eq_id;
        last_flags = static_cast<fles::MicrosliceFlags>(msd.flags);
        // last_sysId = static_cast<fles::Subsystem>(msd.sys_id); // Not used
        last_sys_ver = static_cast<fles::SubsystemFormatFLES>(msd.sys_ver);

        if (false && beVerbose) {
          std::cout << "Microslice " << numMicroslices << ":" << std::endl;

          // TODO: In all of the following, the human-readable names of
          // the enum values should be printed instead of the raw
          // values, unless the raw value does not have a human-readable
          // name, which should be treated as an error.
          //
          // Note: The fixed width integer types may be (and often are)
          // implemented as typedefs to (unsigned) chars or similar,
          // leading to strange output. Hence, we must cast them to a
          // different integer type before printing them. See:
          // https://stackoverflow.com/questions/30447201

          // Print current hdr_id, set of appeared hdr_ids and its size,
          // and the number of changes of hdr_id, all in one line:
          std::cout << "hdr_id: " << static_cast<int>(hdr_id) << " ("
                    << hdr_id_seen.size() << " seen: ";
          for (const auto& seen_hdr_id : hdr_id_seen) {
            std::cout << static_cast<int>(seen_hdr_id) << " ";
          }
          std::cout << ") changes: " << num_hdr_id_changes << std::endl;

          // Print current hdr_ver, set of appeared hdr_vers and its size,
          // and the number of changes of hdr_ver, all in one line:
          std::cout << "hdr_ver: " << static_cast<int>(hdr_ver) << " ("
                    << hdr_ver_seen.size() << " seen: ";
          for (const auto& seen_hdr_ver : hdr_ver_seen) {
            std::cout << static_cast<int>(seen_hdr_ver) << " ";
          }
          std::cout << ") changes: " << num_hdr_ver_changes << std::endl;

          // Print current eq_id, set of appeared eq_ids and its size,
          // and the number of changes of eq_id, all in one line:
          // Note: No static_cast is needed here, because eq_id does not
          // have a custom enum class.
          std::cout << "eq_id: " << static_cast<int>(eq_id) << " ("
                    << eq_id_seen.size() << " seen: ";
          for (const auto& seen_eq_id : eq_id_seen) {
            std::cout << static_cast<int>(seen_eq_id) << " ";
          }
          std::cout << ") changes: " << num_eq_id_changes << std::endl;

          // Print current flags, set of appeared flags and its size,
          // and the number of changes of flags, all in one line:
          // TODO: These are likely or-ed together, so we should rewrite
          // the code to take that into account.
          std::cout << "flags: " << static_cast<int>(flags) << " ("
                    << flags_seen.size() << " seen: ";
          for (const auto& seen_flags : flags_seen) {
            std::cout << static_cast<int>(seen_flags) << " ";
          }
          std::cout << ") changes: " << num_flags_changes << std::endl;

          // Print current sys_id, set of appeared sys_ids and its size,
          // and the number of changes of sys_id, all in one line:
          // TODO: No number of changes is printed, but printing the
          // number of sys_ids per Timeslice and TimesliceComponent might
          // be useful.
          std::cout << "sys_id: " << static_cast<int>(sys_id) << " ("
                    << sys_id_seen.size() << " seen: ";
          for (const auto& seen_sys_id : sys_id_seen) {
            std::cout << static_cast<int>(seen_sys_id) << " ";
          }
          std::cout << ") " << std::endl;

          // Print current sys_ver, set of appeared sys_vers and its size,
          // and the number of changes of sys_ver, all in one line:
          std::cout << "sys_ver: " << static_cast<int>(sys_ver) << " ("
                    << sys_ver_seen.size() << " seen: ";
          for (const auto& seen_sys_ver : sys_ver_seen) {
            std::cout << static_cast<int>(seen_sys_ver) << " ";
          }
          std::cout << ") changes: " << num_sys_ver_changes << std::endl;

          std::cout << std::endl; // Empty line for better readability
        }

        if (!msaWriterOptions.dryRun) {
          if (prefix.size() == 0) {
            std::cerr << "Error: Prefix is empty, should not happen."
                      << " Setting arbitrary prefix." << std::endl;
            prefix = "empty_prefix";
          }
          std::string sys_id_string = fles::to_string(sys_id);
          // eq_id is a uint16_t, and most likely typedefed to some
          // primitive integer type, so likely implicit conversion to
          // that integer type is safe. However, the fixed width
          // integer types are implementation defined, so the correct
          // way to do this likely involves using the PRIu16 format
          // macro.
          std::string eq_id_string = std::to_string(eq_id);
          std::string optionalSequenceIndicator =
              msaWriterOptions.sequence ? "" : "_%n";
          std::string msa_archive_name = prefix + "_" + sys_id_string + "_" +
                                         eq_id_string +
                                         optionalSequenceIndicator + ".msa";
          // TODO: This happens for every microslice, which is really
          // unnecessary and should happen for every TimesliceComponent
          // instead. Only that the values stay constant per
          // TimesliceComponent needs to be checked here.
          if (msaFiles.find(msa_archive_name) == msaFiles.end()) {
            std::unique_ptr<fles::MicrosliceOutputArchive> msaFile =
                std::make_unique<fles::MicrosliceOutputArchive>(
                    msa_archive_name);
            msaFiles[msa_archive_name] = std::move(msaFile);
          }
          msaFiles[msa_archive_name]->put(std::move(ms_ptr));
        }

        if (false && tsaReaderOptions.interactive) {
          std::cout << "Press Enter to continue..." << std::endl;
          std::cin.get();
        }
      }

      if (false && beVerbose) {
        std::cout << "TimesliceComponent " << tsc << " in ts " << numTimeslices
                  << ":" << std::endl;
        std::cout << "Microslice in tsc " << numMicroslices_in_tsc << std::endl;

        // TODO: In all of the following, the human-readable names of
        // the enum values should be printed instead of the raw
        // values, unless the raw value does not have a human-readable
        // name, which should be treated as an error.
        //
        // Note: The fixed width integer types may be (and often are)
        // implemented as typedefs to (unsigned) chars or similar,
        // leading to strange output. Hence, we must cast them to a
        // different integer type before printing them. See:
        // https://stackoverflow.com/questions/30447201

        // Print current hdr_id, set of appeared hdr_ids and its size,
        // and the number of changes of hdr_id, all in one line:
        std::cout << "num hdr_id: " << hdr_id_seen_in_tsc.size() << " seen: ";
        for (const auto& seen_hdr_id : hdr_id_seen_in_tsc) {
          std::cout << static_cast<int>(seen_hdr_id) << " ";
        }
        std::cout << std::endl;

        // Print current hdr_ver, set of appeared hdr_vers and its size,
        // and the number of changes of hdr_ver, all in one line:
        std::cout << "num hdr_ver: " << hdr_ver_seen_in_tsc.size() << " seen: ";
        for (const auto& seen_hdr_ver : hdr_ver_seen_in_tsc) {
          std::cout << static_cast<int>(seen_hdr_ver) << " ";
        }
        std::cout << std::endl;

        // Print current eq_id, set of appeared eq_ids and its size,
        // and the number of changes of eq_id, all in one line:
        // Note: No static_cast is needed here, because eq_id does not
        // have a custom enum class.
        std::cout << "num eq_id: " << eq_id_seen_in_tsc.size() << " seen: ";
        for (const auto& seen_eq_id : eq_id_seen_in_tsc) {
          std::cout << static_cast<int>(seen_eq_id) << " ";
        }
        std::cout << std::endl;

        // Print current flags, set of appeared flags and its size,
        // and the number of changes of flags, all in one line:
        // TODO: These are likely or-ed together, so we should rewrite
        // the code to take that into account.
        std::cout << "num flags: " << flags_seen_in_tsc.size() << " seen: ";
        for (const auto& seen_flags : flags_seen_in_tsc) {
          std::cout << static_cast<int>(seen_flags) << " ";
        }
        std::cout << std::endl;

        // Print current sys_id, set of appeared sys_ids and its size,
        // and the number of changes of sys_id, all in one line:
        // TODO: No number of changes is printed, but printing the
        // number of sys_ids per Timeslice and TimesliceComponent might
        // be useful.
        std::cout << "num sys_id: " << sys_id_seen_in_tsc.size() << " seen: ";
        for (const auto& seen_sys_id : sys_id_seen_in_tsc) {
          std::cout << static_cast<int>(seen_sys_id) << " ";
        }
        std::cout << std::endl;

        // Print current sys_ver, set of appeared sys_vers and its size,
        // and the number of changes of sys_ver, all in one line:
        std::cout << "num sys_ver: " << sys_ver_seen_in_tsc.size() << " seen: ";
        for (const auto& seen_sys_ver : sys_ver_seen_in_tsc) {
          std::cout << static_cast<int>(seen_sys_ver) << " ";
        }
        std::cout << std::endl;

        std::cout << std::endl; // Empty line for better readability
      }
      if (false && tsaReaderOptions.interactive) {
        std::cout << "Press Enter to continue..." << std::endl;
        std::cin.get();
      }
    }
    if (beVerbose) {
      std::cout << "Timeslice " << numTimeslices << ":" << std::endl;
      std::cout << "Microslice in ts " << numMicroslices_in_ts << std::endl;

      // TODO: In all of the following, the human-readable names of
      // the enum values should be printed instead of the raw
      // values, unless the raw value does not have a human-readable
      // name, which should be treated as an error.
      //
      // Note: The fixed width integer types may be (and often are)
      // implemented as typedefs to (unsigned) chars or similar,
      // leading to strange output. Hence, we must cast them to a
      // different integer type before printing them. See:
      // https://stackoverflow.com/questions/30447201

      // Print current hdr_id, set of appeared hdr_ids and its size,
      // and the number of changes of hdr_id, all in one line:
      std::cout << "num hdr_id: " << hdr_id_seen_in_ts.size() << " seen: ";
      for (const auto& seen_hdr_id : hdr_id_seen_in_ts) {
        std::cout << static_cast<int>(seen_hdr_id) << " ";
      }
      std::cout << std::endl;

      // Print current hdr_ver, set of appeared hdr_vers and its size,
      // and the number of changes of hdr_ver, all in one line:
      std::cout << "num hdr_ver: " << hdr_ver_seen_in_ts.size() << " seen: ";
      for (const auto& seen_hdr_ver : hdr_ver_seen_in_ts) {
        std::cout << static_cast<int>(seen_hdr_ver) << " ";
      }
      std::cout << std::endl;

      // Print current eq_id, set of appeared eq_ids and its size,
      // and the number of changes of eq_id, all in one line:
      // Note: No static_cast is needed here, because eq_id does not
      // have a custom enum class.
      std::cout << "num eq_id: " << eq_id_seen_in_ts.size() << " seen: ";
      for (const auto& seen_eq_id : eq_id_seen_in_ts) {
        std::cout << static_cast<int>(seen_eq_id) << " ";
        std::cout << "(" << eq_id_count[seen_eq_id] << " per ts) ";
      }
      std::cout << std::endl;

      // Print current flags, set of appeared flags and its size,
      // and the number of changes of flags, all in one line:
      // TODO: These are likely or-ed together, so we should rewrite
      // the code to take that into account.
      std::cout << "num flags: " << flags_seen_in_ts.size() << " seen: ";
      for (const auto& seen_flags : flags_seen_in_ts) {
        std::cout << static_cast<int>(seen_flags) << " ";
      }
      std::cout << std::endl;

      // Print current sys_id, set of appeared sys_ids and its size,
      // and the number of changes of sys_id, all in one line:
      // TODO: No number of changes is printed, but printing the
      // number of sys_ids per Timeslice and TimesliceComponent might
      // be useful.
      std::cout << "num sys_id: " << sys_id_seen_in_ts.size() << " seen: ";
      for (const auto& seen_sys_id : sys_id_seen_in_ts) {
        std::cout << static_cast<int>(seen_sys_id) << " ";
      }
      std::cout << std::endl;

      // Print current sys_ver, set of appeared sys_vers and its size,
      // and the number of changes of sys_ver, all in one line:
      std::cout << "num sys_ver: " << sys_ver_seen_in_ts.size() << " seen: ";
      for (const auto& seen_sys_ver : sys_ver_seen_in_ts) {
        std::cout << static_cast<int>(seen_sys_ver) << " ";
      }
      std::cout << std::endl;

      std::cout << std::endl; // Empty line for better readability

      if (tsaReaderOptions.interactive) {
        std::cout << "Press Enter to continue..." << std::endl;
        std::cin.get();
      }
      numTimeslices++;
    }
  }

  return EXIT_SUCCESS;
}
