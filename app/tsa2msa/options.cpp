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

  // Parse command line arguments and store them in a variables map
  try {
    // Since we are using positional arguments, we need to use the
    // command_line_parser instead of the parse_command_line
    // function, which only supports named options.
    boost::program_options::store(
        boost::program_options::command_line_parser(argc, argv)
            .options(parser.all)
            .positional(parser.positional)
            .run(),
        parser.vm);
    boost::program_options::notify(parser.vm);
  } catch (const boost::program_options::error& e) {
    parser.errorMessage.push_back("Error: " + std::string(e.what()));
    parsingError = true;
  }

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
