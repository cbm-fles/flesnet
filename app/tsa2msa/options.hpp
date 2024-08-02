#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <boost/program_options.hpp>

#include "msaWriter.hpp"
#include "tsaReader.hpp"

struct genericOptions {
  bool beQuiet;
  bool beVerbose;
  bool showHelp;
  bool showVersion;

  /**
   * @brief Returns the defaults for genericOptions
   *
   * @return genericOptions with default values
   */
  static genericOptions defaults();

  static boost::program_options::options_description
  optionsDescription(genericOptions& options);
};

class options final {
public:
  genericOptions generic;
  tsaReaderOptions tsaReader;
  msaWriterOptions msaWriter;

  bool parsingError;

  options();
  ~options() = default;

  /**
   * @brief Returns the defaults for genericOptions
   *
   * @return genericOptions with default values
   */
  static genericOptions defaults();

  void parseCommandLine(
      int argc,
      char* argv[],
      const boost::program_options::options_description& command_line_options,
      boost::program_options::variables_map& vm,
      std::vector<std::string>& errorMessage);

  /**
   * @brief Handle parsing errors
   *
   * @details This function prints the error messages and usage
   * information to the standard error stream. The information is printed
   * in a way that is consistent with whether the user asked for help
   * and/or verbose output.
   */
  void handleErrors(
      const std::vector<std::string>& errorMessage,
      const boost::program_options::options_description& command_line_options,
      const boost::program_options::options_description&
          visible_command_line_options);

private:
  // Other constructors and operators are deleted because it is not
  // trivial to reason about desired behavior for them.
  options(const options&) = delete;
  options& operator=(const options&) = delete;
  options(options&&) = delete;
  options& operator=(options&&) = delete;
};

#endif // OPTIONS_HPP
