#include <boost/program_options.hpp>

#include "tsaReader.hpp"

tsaReaderOptions tsaReader::defaults() {
  tsaReaderOptions defaults;
  // The following defaults are automatically included in the
  // doxygen documentation as a code snippet:
  // [tsaReaderDefaults]
  defaults.beVerbose = false;
  defaults.interactive = false;
  defaults.input = std::vector<std::string>();
  // [tsaReaderDefaults]
  return defaults;
}

boost::program_options::options_description
tsaReader::optionsDescription(tsaReaderOptions& options, bool hidden) {
  /*
   * Note: boost::program_options throws a runtime exception if multiple
   * options_description objects which contain the same option are
   * combined. Hence, name clashes must be avoided manually.
   */
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
            "How the TSA reader should read the input files."
            " Currently, only 'auto' is supported for using the"
            " TimesliceAutoSource class. In the future, other ways of reading"
            " the input files might be supported, such as by using"
            " TimesliceInputArchiveSequence directly."
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
    // TODO: Remove this workaround once the source is fixed. -> delete
    // this
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
