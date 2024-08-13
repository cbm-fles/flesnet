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
    checkForLogicErrors(parser.vm, parser.errorMessage);
  }

  if (parsingError) {
    handleErrors(parser.errorMessage, parser.all, parser.visible);
  } else if (generic.showHelp) {
    showHelp(parser.all, parser.visible);
  }

  // Since the input files are positional arguments, we need to extract
  // them from the variables map, in contrast to how for the main and
  // the msaWriter all options are automatically set in the
  // msaWriterOptions struct via boost::program_options::value and
  // boost::program_options::bool_switch.
  getTsaReaderOptions(parser.vm, tsaReader);
}

void options::handleErrors(
    const std::vector<std::string>& errorMessage,
    const boost::program_options::options_description& command_line_options,
    const boost::program_options::options_description&
        visible_command_line_options) {

  for (const auto& msg : errorMessage) {
    std::cerr << msg << std::endl;
  }

  if (!generic.showHelp) {
    // Otherwise, the user is expecting a help message, anyway.
    // So, we don't need to inform them about our decision to
    // show them usage information without having been asked.
    std::cerr << "Errors occurred: Printing usage." << std::endl << std::endl;
  }

  if (generic.beVerbose) {
    std::cerr << command_line_options << std::endl;
  } else {
    std::cerr << visible_command_line_options << std::endl;
  }

  if (generic.showHelp) {
    // There was a parsing error, which means that additional
    // options were provided.
    std::cerr << "Error: Ignoring any other options." << std::endl;
  }
}

void options::showHelp(
    const boost::program_options::options_description& command_line_options,
    const boost::program_options::options_description&
        visible_command_line_options) {
  if (generic.beVerbose) {
    std::cout << command_line_options << std::endl;
  } else {
    std::cout << visible_command_line_options << std::endl;
  }
}

void options::checkForLogicErrors(
    const boost::program_options::variables_map& vm,
    std::vector<std::string>& errorMessage) {

  if (errorMessage.size() > 0) {
    // TODO: Handle this case more gracefully.
    std::cerr << "Warning: Expected that so far no error messages are present.";
  }

  // Count passed options:
  unsigned int nPassedOptions = 0;
  for (const auto& option : vm) {
    if (!option.second.defaulted()) {
      nPassedOptions++;
    } else {
      // This option is present, but only because it has a default value
      // which was used. I.e. the user did not provide this option.
    }
  }

  if (nPassedOptions == 0) {
    errorMessage.push_back("Error: No options provided.");
    parsingError = true;
  } else if (generic.showHelp) {
    // If the user asks for help, then we don't need to check for
    // other parsing errors. However, prior to showing the help
    // message, we will inform the user about ignoring any other
    // options and treat this as a parsing error. In contrast to
    // all other parsing errors, the error message will be shown
    // after the help message.
    unsigned int nAllowedOptions = generic.beVerbose ? 2 : 1;
    if (nPassedOptions > nAllowedOptions) {
      parsingError = true;
    }
  } else if (generic.showVersion) {
    if (nPassedOptions > 1) {
      errorMessage.push_back("Error: --version option cannot be combined with"
                             " other options.");
      parsingError = true;
    }
  } else if (vm.count("input") == 0) {
    errorMessage.push_back("Error: No input file provided.");
    parsingError = true;
  }
}
