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

    positional(),
    parsingError(false)
// clang-format on
{
  generic.add(msaWriter);
  hidden.add(tsaReader);

  all.add(generic).add(hidden);
  visible.add(generic);

  // Specify that all positional arguments are input files:
  positional.add("input", -1);
}

void commandLineParser::showHelp(std::ostream& out) const {

  if (opts.generic.beVerbose) {
    out << all << std::endl;
  } else {
    out << visible << std::endl;
  }
}

unsigned int commandLineParser::numParsedOptions() const {
  unsigned int nParsedOptions = 0;
  for (const auto& option : vm) {
    if (!option.second.defaulted()) {
      nParsedOptions++;
    } else {
      // This option is present, but only because it has a default value
      // which was used. I.e. the user did not provide this option.
    }
  }
  return nParsedOptions;
}

void commandLineParser::parse(int argc,
                              char* argv[],
                              std::vector<std::string>& errorMessage) {
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
    parsingError = true;
  }

  if (!parsingError) {
    // Since the input files are positional arguments, we need to extract
    // them from the variables map, in contrast to how for the main and
    // the msaWriter all options are automatically set in the
    // msaWriterOptions struct via boost::program_options::value and
    // boost::program_options::bool_switch.
    getTsaReaderOptions(vm, opts.tsaReader);
  }
}

std::string commandLineParser::getUsage() const {
  std::stringstream ss;
  if (opts.generic.beVerbose) {
    ss << all;
  } else {
    ss << visible;
  }
  return ss.str();
}
