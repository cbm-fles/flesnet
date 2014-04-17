// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "System.hpp"
#include <stdexcept>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <cstring>
#include <vector>
#include <boost/lexical_cast.hpp>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

namespace fles
{
namespace system
{

std::string stringerror(int errnum)
{
    std::vector<char> buf(256);

#ifndef _GNU_SOURCE

    // XSI-compliant
    int err = strerror_r(errnum, buf.data(), buf.size());
    if (err)
        return std::string("Unknown error ") +
               boost::lexical_cast<std::string>(err);

    return std::string(buf.data());

#else

    // GNU-specific
    char* s = strerror_r(errnum, buf.data(), buf.size());
    return std::string(s);

#endif
}

std::string current_username()
{
    uid_t uid = getuid();

    long bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufsize == -1)
        bufsize = 16384;

    std::vector<char> buf(bufsize);

    passwd pwd;
    passwd* result;

    int err = getpwuid_r(uid, &pwd, buf.data(), buf.size(), &result);

    if (err) {
        throw std::runtime_error(stringerror(err));
    }

    if (result) {
        return std::string(pwd.pw_name);
    } else {
        return std::string("unknown");
    }
}

std::string current_hostname()
{
    long bufsize = sysconf(_SC_HOST_NAME_MAX) + 1;
    if (bufsize == -1)
        bufsize = 257;

    std::vector<char> buf(bufsize);

    int err = gethostname(buf.data(), buf.size());
    if (err)
        throw std::runtime_error(stringerror(errno));

    return std::string(buf.data());
}

std::string current_domainname()
{
    long bufsize = 1024;

    std::vector<char> buf(bufsize);

    int err = getdomainname(buf.data(), buf.size());
    if (err)
        throw std::runtime_error(stringerror(errno));

    return std::string(buf.data());
}

} // namespace utility {
} // namespace fles {
