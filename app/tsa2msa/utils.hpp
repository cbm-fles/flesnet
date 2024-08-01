#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>

#include <boost/program_options.hpp>

/**
 * @file utils.hpp
 * @brief Utility functions.
 *
 * \todo There is functionality in other files that could be moved here.
 */

/**
 * @brief Compute the common prefix of a list of strings.
 *
 * @param strings The list of strings.
 * @return The common prefix of the strings.
 */
std::string compute_common_prefix(const std::vector<std::string>& strings);

/**
 * @struct bytesNumber
 * @brief A wrapper around a std::size_t needed by boost::program_options.
 *
 * This struct merely a wrapper around a std::size_t needed by the
 * boost::program_options library to validate human-readable input for
 * the maxBytesPerArchive option in the msaWriterOption. This follows an
 * example provided in the boost::program_options documentation (of e.g.
 * boost version 1.85).
 *
 * \note This struct is not intended to be used directly.
 */
struct bytesNumber {
  std::size_t value;
  bytesNumber(std::size_t value) : value(value) {}
  bytesNumber() : value(0) {}

  // Define the conversion operators following std::size_t behaviour.
  operator bool() const { return value; }
  operator std::size_t() const { return value; }
};

/**
 * @brief Necessary overload the << operator for the bytesNumber struct.
 * @details Defining the << operator is necessary to allow the
 * boost::program_options library to set default values (because it uses
 * this operator to print an options description).
 *
 * @note This function is not intended to be used directly.
 */
std::ostream& operator<<(std::ostream& os, const struct bytesNumber& bN);

/**
 * @brief Validate user input for the maxBytesPerArchive option.
 *
 * This function is only used by the boost::program_options library to
 * validate user input for the maxBytesPerArchive option in the
 * msaWriterOptions struct. It is not intended to be used directly.
 */
void validate(boost::any& v,
              const std::vector<std::string>& values,
              bytesNumber*,
              int);

#endif // UTILS_HPP
