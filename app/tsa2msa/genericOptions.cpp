#include "genericOptions.hpp"

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
