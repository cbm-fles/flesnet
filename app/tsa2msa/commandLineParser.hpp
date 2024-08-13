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
  /**
   * @brief Show help message
   *
   * @details This function prints the help message to the standard output
   * stream. The information is printed in a way that is consistent with
   * whether the user asked for verbose output.
   */
  void showHelp(std::ostream& out);

  /**
   * @brief Handle parsing errors
   *
   * @details This function prints the error messages and usage
   * information to the standard error stream. The information is printed
   * in a way that is consistent with whether the user asked for help
   * and/or verbose output.
   */
  void handleErrors();

  void checkForLogicErrors();

  void parse(int argc, char* argv[]);

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
