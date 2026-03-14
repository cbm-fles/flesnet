#include "options.hpp"

#include "commandLineParser.hpp"

// clang-format off
options::options(const std::string& programDescription) :
  programDescription(programDescription),
  generic(),
  tsaReader(this->generic),
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
