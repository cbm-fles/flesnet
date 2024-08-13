
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
