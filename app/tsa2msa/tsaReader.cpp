#include <boost/program_options.hpp>

#include "tsaReader.hpp"

// Make sure that any changes to the default options are reflected in
// the doxygen documentation of the function.
tsaReaderOptions defaultTsaReaderOptions() {
  tsaReaderOptions options;
  options.beVerbose = false;
  options.interactive = false;
  options.input = std::vector<std::string>();
  return options;
}

boost::program_options::options_description
getTsaReaderOptionsDescription(tsaReaderOptions& options, bool hidden) {
  if (!hidden) {
    boost::program_options::options_description desc(
        "Additional TSA Reader Options:");
    // No visible options for the tsaReader for now
    // TODO: Specify in options description that there is no options in case of
    // later call.
    return desc;
  } else {
    boost::program_options::options_description desc("TSA Reader Options");
    desc.add_options()
        // clang-format off
        (
          // Following an example in the Boost Program Options
          // Tutorial, the hidden input-file option below is used as a
          // helper for interpreting all positional arguments as input
          // files.
          "input,i",
            boost::program_options::value<std::vector<std::string>>(
              &options.input),
            "Input file(s) to read from."
        )
        (
            "interactive",
              boost::program_options::value<bool>(
                &options.interactive)
                  -> default_value(false),
            "Whether to run the TSA reader in interactive mode."
            " Useful for debugging .tsa files or the tsaReader itself."
            " It is recommended to use the 'verbose' option in conjunction"
            " with this option."
        )
        (
           "readingMode",
              boost::program_options::value<std::string>(
                &options.readingMethod)
                  -> default_value("auto"),
            "Reading mode for the TSA reader."
            " Currently, only 'auto' is supported for using the"
            " TimesliceAutoSource class."
        )
        // clang-format on
        ;
    return desc;
  }
}

void getTsaReaderOptions(const boost::program_options::variables_map& vm,
                         tsaReaderOptions& tsaReaderOptions) {
  tsaReaderOptions.beVerbose = vm["verbose"].as<bool>();
}

tsaReaderValidator::tsaReaderValidator()
    : time_monotonically_increasing(true), time_strict_monotonicity(true),
      index_monotonically_increasing(true), index_strict_monotonicity(true),
      num_core_microslices_constant(true), num_components_constant(true),

      nStrictMonotonicityViolationsTime(0),
      nStrictMonotonicityViolationsIndex(0), nMonotonicityViolationsTime(0),
      nMonotonicityViolationsIndex(0), nNumCoreMicroslicesViolations(0),
      nNumComponentsViolations(0),

      last_timeslice_start_time(0), last_timeslice_index(0),
      last_timeslice_num_core_microslices(0), last_timeslice_num_components(0),

      nTimeslices(0) {}

int tsaReaderValidator::validate(
    const std::unique_ptr<fles::Timeslice>& timeslice) {
  if (!timeslice) {
    std::cerr << "Error: No timeslice received." << std::endl;
    return EX_SOFTWARE;
  }

  uint64_t timesliceIndex = timeslice->index();
  uint64_t numCoreMicroslices = timeslice->num_core_microslices();
  uint64_t numComponents = timeslice->num_components();
  uint64_t start_time = timeslice->start_time();
  if (nTimeslices != 0) {
    if (start_time < last_timeslice_start_time) {
      time_monotonically_increasing = false;
      time_strict_monotonicity = false;
      nMonotonicityViolationsTime++;
      nStrictMonotonicityViolationsTime++;
    } else if (start_time == last_timeslice_start_time) {
      time_strict_monotonicity = false;
      nStrictMonotonicityViolationsTime++;
    }
  }
  last_timeslice_start_time = start_time;

  if (nTimeslices != 0) {
    if (timesliceIndex < last_timeslice_index) {
      index_monotonically_increasing = false;
      index_strict_monotonicity = false;
      nMonotonicityViolationsIndex++;
      nStrictMonotonicityViolationsIndex++;
    } else if (timesliceIndex == last_timeslice_index) {
      index_strict_monotonicity = false;
      nStrictMonotonicityViolationsIndex++;
    }
  }
  last_timeslice_index = timesliceIndex;

  if (nTimeslices != 0 &&
      numCoreMicroslices != last_timeslice_num_core_microslices) {
    num_core_microslices_constant = false;
    nNumCoreMicroslicesViolations++;
  }
  last_timeslice_num_core_microslices = numCoreMicroslices;
  if (nTimeslices != 0 && numComponents != last_timeslice_num_components) {
    num_components_constant = false;
    nNumComponentsViolations++;
  }
  last_timeslice_num_components = numComponents;
  nTimeslices++;

  return EX_OK;
}

void tsaReaderValidator::printVerboseIntermediateResult() {
  std::cout << "Timeslice number: " << nTimeslices << std::endl;
  std::cout << "Timeslice index: " << last_timeslice_index << std::endl;
  std::cout << "Number of core microslices: "
            << last_timeslice_num_core_microslices << std::endl;
  std::cout << "Number of components: " << last_timeslice_num_components
            << std::endl;
  std::cout << "Start time: " << last_timeslice_start_time << std::endl;
  if (!time_monotonically_increasing) {
    std::cout << "Warning: Timeslice start time is not monotonically "
                 "increasing."
              << std::endl;
    std::cout << "Number of monotonicity violations: "
              << nMonotonicityViolationsTime << std::endl;
  } else if (!time_strict_monotonicity) {
    std::cout << "Error: Timeslice start time is not strictly "
                 "monotonically increasing."
              << std::endl;
    std::cout << "Number of strict monotonicity violations: "
              << nStrictMonotonicityViolationsTime << std::endl;
  }
  if (!index_monotonically_increasing) {
    std::cout << "Warning: Timeslice index is not monotonically increasing."
              << std::endl;
    std::cout << "Number of monotonicity violations: "
              << nMonotonicityViolationsIndex << std::endl;
  } else if (!index_strict_monotonicity) {
    std::cout << "Error: Timeslice index is not strictly monotonically "
                 "increasing."
              << std::endl;
    std::cout << "Number of strict monotonicity violations: "
              << nStrictMonotonicityViolationsIndex << std::endl;
  }
  if (!num_core_microslices_constant) {
    std::cout << "Warning: Number of core microslices is not constant."
              << std::endl;
    std::cout << "Number of violations: " << nNumCoreMicroslicesViolations
              << std::endl;
  }
  if (!num_components_constant) {
    std::cout << "Warning: Number of components is not constant." << std::endl;
    std::cout << "Number of violations: " << nNumComponentsViolations
              << std::endl;
  }
}

void tsaReaderValidator::printSummary() {

  std::cout << "Number of timeslices: " << nTimeslices << std::endl;
  if (!time_monotonically_increasing) {
    std::cerr
        << "Warning: Timeslice start time is not monotonically increasing."
        << std::endl;
  } else if (!time_strict_monotonicity) {
    std::cerr << "Error: Timeslice start time is not strictly monotonically "
                 "increasing."
              << std::endl;
  }
  if (!index_monotonically_increasing) {
    std::cerr << "Warning: Timeslice index is not monotonically increasing."
              << std::endl;
  } else if (!index_strict_monotonicity) {
    std::cerr << "Error: Timeslice index is not strictly monotonically "
                 "increasing."
              << std::endl;
  }
}

tsaReader::tsaReader(const tsaReaderOptions options)
    : options(options), validator(), source(nullptr), nTimeslices(0),
      last_duration(), // purposefully left unspecified
      total_duration(0) {
  initSource(options.input);
}

std::unique_ptr<fles::Timeslice> tsaReader::handleEosError() {
  // TODO: Make strict_eos an tsaReaderOption:
  bool strict_eos = false;
  if (strict_eos) {
    std::cerr << "Error: No timeslice received." << std::endl;
    if (options.beVerbose) {
      std::cout << "Next eos: " << source->eos() << std::endl;
    }
    throw std::runtime_error("No timeslice received.");
  } else {
    // Ignore the fact that eos did not indicate the end of the
    // file and return nullptr as if nothing extraordinary
    // happened:
    return nullptr;
  }
}

void tsaReader::printDuration() {
  if (nTimeslices == 0) {
    std::cerr << "Warning: Cannot print duration since no timeslices "
                 "have been read yet."
              << std::endl;
    return;
  }
  std::cout << "Elapsed time: " << last_duration.count() << "s"
            << " ("
            << std::chrono::duration_cast<std::chrono::seconds>(total_duration)
                       .count() /
                   static_cast<double>(nTimeslices)
            << "s avg.)" << std::endl;
}

std::unique_ptr<fles::Timeslice> tsaReader::read() {
  if (!source) {
    std::cerr << "Error: No source set." << std::endl;
    throw std::runtime_error("No source set.");
  }
  try {
    bool eos = source->eos();
    if (eos) {
      return nullptr;
    }

    auto start = std::chrono::steady_clock::now();
    std::unique_ptr<fles::Timeslice> timeslice = source->get();
    auto end = std::chrono::steady_clock::now();

    // Sometimes the source does not return a timeslice even if it
    // did not indicate eos.
    // TODO: Remove this workaround once the source is fixed.
    if (!timeslice) {
      return handleEosError();
    }

    // Adjust statistics:
    nTimeslices++;
    auto duration = end - start;
    total_duration += duration;
    last_duration = duration;

    // TODO: Add option to validate the timeslice (or not):
    validator.validate(timeslice);

    if (options.beVerbose) {
      printDuration();
      validator.printVerboseIntermediateResult();
    }

    if (options.interactive) {
      // Wait for user input to continue:
      std::string dummy;
      std::cout << "Press Enter to continue..." << std::endl;
      std::getline(std::cin, dummy);
    }

    return timeslice;

  } catch (const std::exception& e) {
    std::cerr << "tsaReader::read(): Error: " << e.what() << std::endl;
    throw e;
  }
}

void tsaReader::setSource(std::unique_ptr<fles::TimesliceSource> s) {
  // The move assignment of a unique pointer calls the destructor of
  // the object that was previously held by the unique pointer (when
  // necessary) and assigns the new object to the unique pointer.
  source = std::move(s);
}

void tsaReader::initAutoSource(const std::vector<std::string>& inputs) {

  // Check if any of the input files are given as a glob pattern
  // as long as the implementation is not fully reliable:
  // TODO: Remove this check once the implementation is reliable.
  for (const auto& input : inputs) {
    // This is a very simple check that triggers even if the special
    // characters are escaped, but since it only triggers a warning,
    // it is acceptable for now.
    if (input.find('*') != std::string::npos ||
        input.find('?') != std::string::npos ||
        input.find('[') != std::string::npos) {
      std::cerr
          << "Warning: While TimesliceAutoSource supports glob patterns\n"
          << "         for input files, the current implementation seems\n"
          << "         to have issues with glob patterns and occasionally\n"
          << "         provide Timeslices in the wrong order. It is\n"
          << "         recommended to let your shell expand the glob.\n"
          << std::endl;
      break;
    }
  }

  // Join the input files into a ;-separated string, as expected by
  // the TimesliceAutoSource:
  std::string input = "";
  for (auto i = inputs.begin(); i != inputs.end(); i++) {
    if (i != inputs.begin()) {
      input += ";";
    }
    input += *i;
  }

  std::unique_ptr<fles::TimesliceAutoSource> s =
      std::make_unique<fles::TimesliceAutoSource>(input);
  setSource(std::move(s));
}

void tsaReader::initSource(const std::vector<std::string>& inputs) {
  if (options.readingMethod == "auto") {
    initAutoSource(inputs);
  } else {
    throw std::runtime_error("Invalid reading method");
  }
}
