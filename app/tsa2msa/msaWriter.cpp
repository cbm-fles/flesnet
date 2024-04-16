#include "msaWriter.hpp"

#include <boost/program_options.hpp>

msaWriter::msaWriter(const msaWriterOptions& options) : options(options) {
  // Do nothing for now
}

msaWriterOptions defaultMsaWriterOptions() {
  // Make sure to update doxygen documentation at declaration site if
  // the default values are changed.
  return {
      false, // dryRun
      false, // beVerbose
      ""     // prefix
  };
}

boost::program_options::options_description
getMsaWriterOptionsDescription(msaWriterOptions& options, bool hidden) {
  if (hidden) {
    boost::program_options::options_description desc(
        "Additional MSA Writer Options");
    return desc; // No hidden options for now
  } else {
    boost::program_options::options_description desc("MSA Writer Options");
    // clang-format off
    desc.add_options()
        ("dry-run,d",
          boost::program_options::bool_switch(&options.dryRun),
          "Dry run (do not write any msa files)") 
        ("prefix,p",
          boost::program_options::value<std::string>(&options.prefix)
              -> default_value(""),
          "Output prefix for msa files")
        ;
    // clang-format on
    return desc;
  }
}

void getNonSwitchMsaWriterOptions(
    [[maybe_unused]] // Remove this line if the parameter is used
    const boost::program_options::variables_map& vm,
    [[maybe_unused]] // Remove this line if the parameter is used
    msaWriterOptions& msaWriterOptions) {
  // No exclusive non-switch options for now
}

unsigned int msaWriterNumberOfExclusiveBooleanSwitches() { return 1; }
