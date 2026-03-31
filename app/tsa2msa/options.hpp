#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <boost/program_options.hpp>

#include "genericOptions.hpp"
#include "msaWriter.hpp"
#include "tsaReader.hpp"

class options final {
public:
  const std::string programDescription;
  genericOptions generic;
  tsaReaderOptions tsaReader;
  msaWriterOptions msaWriter;

  options(const std::string& programDescription);
  ~options() = default;

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
