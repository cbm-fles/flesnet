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

/**
 * @brief Clean up a path by removing everything except for the file name.
 *
 * @details This function is used to ensure that the output file names
 * are placed in the current directory and not unexpectedly in the
 * directory of the input files.
 *
 * \todo Use boost::filesystem::path instead of the following hacky
 * solution.
 *
 * @note This function does not work if the path separator is not '/'.
 * However, a compile time check is performed to ensure that the machine
 * runs a linux. Possibly more portable solutions could be implemented.
 *
 * @param path The path to clean up.
 *
 */
void clean_up_path(std::string& path);

#endif // UTILS_HPP
