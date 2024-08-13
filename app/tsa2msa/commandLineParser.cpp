
#include "commandLineParser.hpp"

commandLineParser::commandLineParser(options& opts)
    : opts(opts), generic(genericOptions::optionsDescription(opts.generic)) {}
