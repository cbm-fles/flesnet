#include "options.hpp"

#include "commandLineParser.hpp"

genericOptions::genericOptions()
    : // clang-format off
  // [genericDefaults]
  beQuiet(false),
  beVerbose(false),
  showHelp(false),
  showVersion(false)
  // [genericDefaults]
// clang-format on
{}

boost::program_options::options_description
genericOptions::optionsDescription(bool hidden) {
  if (hidden) {
    // no hidden generic options so far
    return boost::program_options::options_description();
  }

  boost::program_options::options_description desc("Generic options");
  desc.add_options()
      // clang-format off
      ("quiet,q",
        boost::program_options::bool_switch(&this->beQuiet)
            ->default_value(this->beQuiet),
        "suppress all output")
      ("verbose,v",
        boost::program_options::bool_switch(&this->beVerbose)
            ->default_value(this->beVerbose),
        "enable verbose output")
      ("help,h",
        boost::program_options::bool_switch(&this->showHelp)
            ->default_value(this->showHelp),
        "produce help message\n"
        "  (combine with --verbose to see all options)")
      ("version,V",
        boost::program_options::bool_switch(&this->showVersion)
            ->default_value(this->showVersion),
        "produce version message")
      // clang-format on
      ;
  return desc;
}

// clang-format off
options::options(const std::string& programDescription) :
  programDescription(programDescription),
  generic(),
  tsaReader(),
  msaWriter()
// clang-format on
{}

bool options::areValid(std::vector<std::string>& errorMessage) const {
  if (generic.showHelp || generic.showVersion) {
    // It might be sensible to expect that no other options are provided
    // by the user when they ask for help or version information, but
    // this is not easily enforceable in a dynamic way (in particular,
    // without access to how user input is parsed). Hence, we don't
    // check for this here.
    return true;
  }

  if (tsaReader.input.empty()) {
    errorMessage.push_back("No input file specified.");
    return false;
  }

  return true;
}
