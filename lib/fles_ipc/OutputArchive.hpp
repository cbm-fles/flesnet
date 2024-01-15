// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::OutputArchive template class.
#pragma once

#include "ArchiveDescriptor.hpp"
#include "Sink.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/filter/zstd.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <fstream>
#include <string>

namespace fles {

/**
 * \brief The OutputArchive class serializes data sets to an output file.
 */
template <class Base, class Derived, ArchiveType archive_type>
class OutputArchive : public Sink<Base> {
public:
  /**
   * \brief Construct an output archive object, open the given archive file
   * for writing, and write the archive descriptor.
   *
   * \param filename File name of the archive file
   * \param compression Compression type to use
   */
  explicit OutputArchive(const std::string& filename)
      : ofstream_(filename, std::ios::binary), descriptor_{archive_type} {

    oarchive_ = std::make_unique<boost::archive::binary_oarchive>(ofstream_);

    *oarchive_ << descriptor_;
  }

  /// Delete copy constructor (non-copyable).
  OutputArchive(const OutputArchive&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const OutputArchive&) = delete;

  ~OutputArchive() override = default;

  /// Store an item.
  void put(std::shared_ptr<const Base> item) override { do_put(*item); }

private:
  std::ofstream ofstream_;
  std::unique_ptr<boost::archive::binary_oarchive> oarchive_;
  LegacyArchiveDescriptor descriptor_;

  void do_put(const Derived& item) { *oarchive_ << item; }
  // TODO(Jan): Solve this without the additional alloc/copy operation
};

} // namespace fles
