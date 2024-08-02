#include "options.hpp"

options::options() : global(options::defaults()) {}

globalOptions options::defaults() {
  globalOptions defaults;
  // [globalDefaults]
  defaults.beQuiet = false;
  defaults.beVerbose = false;
  defaults.showHelp = false;
  defaults.showVersion = false;
  // [globalDefaults]
  return defaults;
}
