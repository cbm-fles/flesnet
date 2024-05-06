#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>

std::string compute_common_prefix(const std::vector<std::string>& strings);

// Remove everything except for the file name:
//
// TODO: Use boost::filesystem::path instead of the following hacky
// code.
void clean_up_path(std::string& path);

#endif // UTILS_HPP
