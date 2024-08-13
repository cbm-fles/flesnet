#include <iostream>

#include "commandLineParser.hpp"
#include "msaWriter.hpp"
#include "tsaReader.hpp"

commandLineParser::commandLineParser(options& opts)
    : // clang-format off
    opts(opts),
    generic(genericOptions::optionsDescription(opts.generic)),

    hidden("Hidden options"),
    visible(opts.programDescription + "\n" + "Command line options"),
    all(opts.programDescription + "\n" + "Command line options"),

    tsaReader(tsaReader::optionsDescription(opts.tsaReader,
        /* hidden */ true)),
    msaWriter(msaWriter::optionsDescription(opts.msaWriter,
        /* hidden */ false)),

    positional()
// clang-format on
{
  generic.add(msaWriter);
  hidden.add(tsaReader);

  all.add(generic).add(hidden);
  visible.add(generic);

  // Specify that all positional arguments are input files:
  positional.add("input", -1);
}

void commandLineParser::showHelp(std::ostream& out) {

  if (opts.generic.beVerbose) {
    out << all << std::endl;
  } else {
    out << visible << std::endl;
  }
}

void commandLineParser::handleErrors() {

  for (const auto& msg : errorMessage) {
    std::cerr << msg << std::endl;
  }

  if (!opts.generic.showHelp) {
    // Otherwise, the user is expecting a help message, anyway.
    // So, we don't need to inform them about our decision to
    // show them usage information without having been asked.
    std::cerr << "Errors occurred: Printing usage." << std::endl << std::endl;
  }

  showHelp(std::cerr);

  if (opts.generic.showHelp) {
    // There was a parsing error, which means that additional
    // options were provided.
    std::cerr << "Error: Ignoring any other options." << std::endl;
  }
}

void commandLineParser::checkForLogicErrors() {

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
    opts.parsingError = true;
  } else if (opts.generic.showHelp) {
    // If the user asks for help, then we don't need to check for
    // other parsing errors. However, prior to showing the help
    // message, we will inform the user about ignoring any other
    // options and treat this as a parsing error. In contrast to
    // all other parsing errors, the error message will be shown
    // after the help message.
    unsigned int nAllowedOptions = opts.generic.beVerbose ? 2 : 1;
    if (nPassedOptions > nAllowedOptions) {
      opts.parsingError = true;
    }
  } else if (opts.generic.showVersion) {
    if (nPassedOptions > 1) {
      errorMessage.push_back("Error: --version option cannot be combined with"
                             " other options.");
      opts.parsingError = true;
    }
  } else if (vm.count("input") == 0) {
    errorMessage.push_back("Error: No input file provided.");
    opts.parsingError = true;
  }
}

void commandLineParser::parse(int argc, char* argv[]) {
  // Parse command line arguments and store them in a variables map
  try {
    // Since we are using positional arguments, we need to use the
    // command_line_parser instead of the parse_command_line
    // function, which only supports named options.
    boost::program_options::store(
        boost::program_options::command_line_parser(argc, argv)
            .options(all)
            .positional(positional)
            .run(),
        vm);
    boost::program_options::notify(vm);
  } catch (const boost::program_options::error& e) {
    errorMessage.push_back("Error: " + std::string(e.what()));
    opts.parsingError = true;
  }

  // Check for further parsing errors:
  if (!opts.parsingError) {
    checkForLogicErrors();
  }

  if (opts.parsingError) {
    handleErrors();
  } else if (opts.generic.showHelp) {
    showHelp(std::cout);
  }

  // Since the input files are positional arguments, we need to extract
  // them from the variables map, in contrast to how for the main and
  // the msaWriter all options are automatically set in the
  // msaWriterOptions struct via boost::program_options::value and
  // boost::program_options::bool_switch.
  getTsaReaderOptions(vm, opts.tsaReader);
}
