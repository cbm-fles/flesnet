#include <boost/program_options.hpp>

#include "tsaReader.hpp"

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

