// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::ArchiveDescriptor class.
#pragma once

#include "System.hpp"
#include <chrono>
#include <string>
#include <boost/serialization/access.hpp>
#include <boost/serialization/version.hpp>

namespace fles
{

/// The archive type enum (e.g., timeslice, microslice)
enum class ArchiveType { TimesliceArchive, MicrosliceArchive };

template <class Base, class Derived, ArchiveType archive_type>
class InputArchive;

/**
 * \brief The ArchiveDescriptor class contains metadata on an archive.
 *
 * This class class precedes a stream of serialized StorableTimeslice or
 * StorableMicroslice objects.
 */
class ArchiveDescriptor
{
public:
    /**
     * \brief Public constructor.
     *
     * \param archive_type The type of archive (e.g., timeslice, microslice).
     */
    ArchiveDescriptor(ArchiveType archive_type) : _archive_type(archive_type)
    {
        _time_created = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
        _hostname = fles::system::current_hostname();
        _username = fles::system::current_username();
    }

    /// Retrieve the type of archive.
    ArchiveType archive_type() const { return _archive_type; }

    /// Retrieve the time of creation of the archive.
    std::time_t time_created() const { return _time_created; }

    /// Retrieve the hostname of the machine creating the archive.
    std::string hostname() const { return _hostname; }

    /// Retrieve the hostname of the machine creating the archive.
    std::string username() const { return _username; }

private:
    friend class boost::serialization::access;
    /// Provide boost serialization access.
    template <class Base, class Derived, ArchiveType archive_type>
    friend class InputArchive;

    ArchiveDescriptor(){};

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        if (version > 0) {
            ar& _archive_type;
        } else {
            _archive_type = ArchiveType::TimesliceArchive;
        };
        ar& _time_created;
        ar& _hostname;
        ar& _username;
    }

    ArchiveType _archive_type;
    std::time_t _time_created = std::time_t();
    std::string _hostname;
    std::string _username;
};

} // namespace fles {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
BOOST_CLASS_VERSION(fles::ArchiveDescriptor, 1)
#pragma GCC diagnostic pop
