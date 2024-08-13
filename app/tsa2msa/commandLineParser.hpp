#ifndef COMMANDLINEPARSER_HPP
#define COMMANDLINEPARSER_HPP

#include "options.hpp"

class commandLineParser {

  options& opts;

public:
  boost::program_options::options_description generic;

public:
  commandLineParser(options& opts);
  ~commandLineParser() = default;

private:
  // Delete other constructors:
  commandLineParser() = delete;
  commandLineParser(const commandLineParser&) = delete;
  commandLineParser(commandLineParser&&) = delete;
  commandLineParser& operator=(const commandLineParser&) = delete;
  commandLineParser& operator=(commandLineParser&&) = delete;
};

#endif // COMMANDLINEPARSER_HPP
