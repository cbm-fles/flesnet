#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <boost/program_options.hpp>

#include "msaWriter.hpp"
#include "tsaReader.hpp"

class genericOptions : public optionsGroup {
public:
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

  boost::program_options::options_description
  optionsDescription(bool hidden) override;

  genericOptions() = default;
  ~genericOptions() override = default;
};

class options final {
public:
  const std::string programDescription;
  genericOptions generic;
  tsaReaderOptions tsaReader;
  msaWriterOptions msaWriter;

  options(const std::string& programDescription);
  ~options() = default;

  /**
   * @brief Returns the defaults for genericOptions
   *
   * @return genericOptions with default values
   */
  static genericOptions defaults();

  bool areValid(std::vector<std::string>& errorMessage) const;

private:
  // Other constructors and operators are deleted because it is not
  // trivial to reason about desired behavior for them.
  options(const options&) = delete;
  options& operator=(const options&) = delete;
  options(options&&) = delete;
  options& operator=(options&&) = delete;
};

#endif // OPTIONS_HPP
