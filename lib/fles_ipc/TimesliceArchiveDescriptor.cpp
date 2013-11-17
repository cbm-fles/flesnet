// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceArchiveDescriptor.hpp"
#include <stdexcept>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

namespace fles
{

std::string TimesliceArchiveDescriptor::current_username() const
{
    //    passwd pwd;
    //    passwd* result;
    //
    //    size_t bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
    //    if (bufsize == -1)
    //        bufsize = 16384;
    //
    //    uid_t uid = getuid();
    //
    //    char* buf = new char[bufsize];
    //
    //    int err = getpwuid_r(uid, &pwd, buf, bufsize, &result);
    //
    //    if (err) {
    //        throw std::runtime_error("bla");
    //    }
    //
    //
    //    std::string pw_name("");
    //    if (!err) {
    //        if (result) {
    //            pw_name = std::string(pwd->pw_name);
    //        }
    //    }
    //    delete [] buf;
    //    delete pwd;

    // continue here: make utility stringerror(int errnum)...
    return std::string("---");
}

} // namespace fles {
