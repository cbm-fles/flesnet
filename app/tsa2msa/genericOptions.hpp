#ifndef GENERICOPTIONS_HPP
#define GENERICOPTIONS_HPP

#include <boost/program_options.hpp>

#include "optionsGroup.hpp"

class genericOptions : public optionsGroup {
public:
  bool beQuiet;
  bool beVerbose;
  bool showHelp;
  bool showVersion;

  boost::program_options::options_description
  optionsDescription(bool hidden) override;

  /**
   * @brief Returns the defaults for genericOptions
   *
   * @return genericOptions with default values
   */
  genericOptions();
  ~genericOptions() override = default;
};

#endif // GENERICOPTIONS_HPP
