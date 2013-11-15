// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <chrono>
#include <string>
#include <boost/serialization/access.hpp>

namespace fles
{

//! The TimesliceArchiveDescriptor precedes a stream of serialized
// StorableTimeslice objects.
class TimesliceArchiveDescriptor
{
public:
    TimesliceArchiveDescriptor()
        : _time_created(std::chrono::system_clock::to_time_t(
              std::chrono::system_clock::now()))
    {
        _hostname = std::string("---");
        _username = current_username();
        // TODO: add hostname, username
    }

    /// Retrieve the time of creation of the archive.
    std::time_t time_created() const
    {
        return _time_created;
    }

    /// Retrieve the hostname of the machine creating the archive.
    std::string hostname() const
    {
        return _hostname;
    }

    /// Retrieve the hostname of the machine creating the archive.
    std::string username() const
    {
        return _username;
    }

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int /* version */)
    {
        ar& _time_created;
        ar& _hostname;
        ar& _username;
    }

    std::string current_username() const;

    std::time_t _time_created;
    std::string _hostname;
    std::string _username;
};

} // namespace fles {
