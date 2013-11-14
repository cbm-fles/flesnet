// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <chrono>
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
    }

    /// Retrieve the time of creation of the archive.
    std::time_t time_created() const;
    // TODO: add hostname, username etc.

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int /* version */)
    {
        ar& _time_created;
    }

    std::time_t _time_created;
};

} // namespace fles {
