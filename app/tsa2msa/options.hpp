#ifndef OPTIONS_HPP
#define OPTIONS_HPP

struct globalOptions {
  bool beQuiet;
  bool beVerbose;
  bool showHelp;
  bool showVersion;
};

class options final {
public:
  globalOptions global;

  options();
  ~options() = default;

  /**
   * @brief Returns the defaults for globalOptions
   *
   * @return globalOptions with default values
   */
  static globalOptions defaults();

private:
  // Other constructors and operators are deleted because it is not
  // trivial to reason about desired behavior for them.
  options(const options&) = delete;
  options& operator=(const options&) = delete;
  options(options&&) = delete;
  options& operator=(options&&) = delete;
};

#endif // OPTIONS_HPP
