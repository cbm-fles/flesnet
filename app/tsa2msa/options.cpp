#include "options.hpp"

#include "commandLineParser.hpp"

genericOptions genericOptions::defaults() {
  genericOptions defaults;
  // [genericDefaults]
  defaults.beQuiet = false;
  defaults.beVerbose = false;
  defaults.showHelp = false;
  defaults.showVersion = false;
  // [genericDefaults]
  return defaults;
}

boost::program_options::options_description
genericOptions::optionsDescription(genericOptions& options) {
  boost::program_options::options_description desc("Generic options");
  desc.add_options()
      // clang-format off
      ("quiet,q",
        boost::program_options::bool_switch(&options.beQuiet),
        "suppress all output")
      ("verbose,v",
        boost::program_options::bool_switch(&options.beVerbose),
        "enable verbose output")
      ("help,h",
        boost::program_options::bool_switch(&options.showHelp),
        "produce help message\n"
        "  (combine with --verbose to see all options)")
      ("version,V",
        boost::program_options::bool_switch(&options.showVersion),
        "produce version message")
      // clang-format on
      ;
  return desc;
}

// clang-format off
options::options(const std::string& programDescription) :
  programDescription(programDescription),
  generic(genericOptions::defaults()),
  tsaReader(tsaReader::defaults()),
  msaWriter(msaWriter::defaults()),
  parsingError(false)
// clang-format on
{}

void options::parseCommandLine(int argc, char* argv[]) {

  commandLineParser parser(*this);

  // Parse command line options:
  parser.parse(argc, argv);

  // Check for further parsing errors:
  if (!parsingError) {
    parser.checkForLogicErrors();
  }

  if (parsingError) {
    parser.handleErrors();
  } else if (generic.showHelp) {
    parser.showHelp(std::cout);
  }

  // Since the input files are positional arguments, we need to extract
  // them from the variables map, in contrast to how for the main and
  // the msaWriter all options are automatically set in the
  // msaWriterOptions struct via boost::program_options::value and
  // boost::program_options::bool_switch.
  getTsaReaderOptions(parser.vm, tsaReader);
}
