#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>

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

#endif // UTILS_HPP
