#ifndef COMMANDLINEPARSER_HPP
#define COMMANDLINEPARSER_HPP

#include "options.hpp"

class commandLineParser {

  options& opts;

public:
  boost::program_options::options_description generic;
  boost::program_options::options_description hidden;
  boost::program_options::options_description visible;
  boost::program_options::options_description all;

  boost::program_options::options_description tsaReader;
  boost::program_options::options_description msaWriter;

  boost::program_options::positional_options_description positional;

  boost::program_options::variables_map vm;
  std::vector<std::string> errorMessage;

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
