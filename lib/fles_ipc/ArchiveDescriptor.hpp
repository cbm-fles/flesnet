// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::ArchiveDescriptor class.
#pragma once

#include "System.hpp"
#include <boost/serialization/access.hpp>
#include <boost/serialization/version.hpp>
#include <chrono>
#include <string>

namespace fles {

/// The archive type enum (e.g., timeslice, microslice)
enum class ArchiveType {
  TimesliceArchive,
  MicrosliceArchive,
  RecoResultsArchive,
  QaDataArchive
};

/// The archive compression enum
enum class ArchiveCompression { None, Zstd };

template <class Base, class Derived, ArchiveType archive_type>
class InputArchive;

/**
 * \brief The ArchiveDescriptor class contains metadata on an archive.
 *
 * This class class precedes a stream of serialized StorableTimeslice or
 * StorableMicroslice objects.
 */
class ArchiveDescriptor {
public:
  /**
   * \brief Public constructor.
   *
   * \param archive_type The type of archive (e.g., timeslice, microslice).
   * \param archive_compression archive_compression.
   */
  explicit ArchiveDescriptor(
      ArchiveType archive_type,
      ArchiveCompression archive_compression = ArchiveCompression::None)
      : archive_type_(archive_type), archive_compression_(archive_compression) {
    time_created_ =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    hostname_ = fles::system::current_hostname();
    username_ = fles::system::current_username();
  }

  /// Retrieve the type of archive.
  [[nodiscard]] ArchiveType archive_type() const { return archive_type_; }

  /// Retrieve the compression type of archive.
  [[nodiscard]] ArchiveCompression archive_compression() const {
    return archive_compression_;
  }

  /// Retrieve the time of creation of the archive.
  [[nodiscard]] std::time_t time_created() const { return time_created_; }

  /// Retrieve the hostname of the machine creating the archive.
  [[nodiscard]] std::string hostname() const { return hostname_; }

  /// Retrieve the hostname of the machine creating the archive.
  [[nodiscard]] std::string username() const { return username_; }

private:
  friend class boost::serialization::access;
  /// Provide boost serialization access.
  template <class Base, class Derived, ArchiveType archive_type>
  friend class InputArchive;
  template <class Base, class Derived, ArchiveType archive_type>
  friend class InputArchiveLoop;
  template <class Base, class Derived, ArchiveType archive_type>
  friend class InputArchiveSequence;

  ArchiveDescriptor() = default;

  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    if (version > 0) {
      ar& archive_type_;
    } else {
      archive_type_ = ArchiveType::TimesliceArchive;
    };
    if (version > 1) {
      ar& archive_compression_;
    } else {
      archive_compression_ = ArchiveCompression::None;
    };
    ar& time_created_;
    ar& hostname_;
    ar& username_;
  }

  ArchiveType archive_type_{};
  ArchiveCompression archive_compression_{};
  std::time_t time_created_ = std::time_t();
  std::string hostname_;
  std::string username_;
};

} // namespace fles

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
BOOST_CLASS_VERSION(fles::ArchiveDescriptor, 2)
#pragma GCC diagnostic pop
