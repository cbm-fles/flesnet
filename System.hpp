// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include <string>

namespace fles
{
namespace system
{

std::string stringerror(int errnum);
std::string current_username();
std::string current_hostname();
std::string current_domainname();

} // namespace utility {
} // namespace fles {
