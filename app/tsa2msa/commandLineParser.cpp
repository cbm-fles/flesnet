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
