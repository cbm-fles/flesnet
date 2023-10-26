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
   */
  explicit OutputArchive(
      const std::string& filename,
      ArchiveCompression compression = ArchiveCompression::None)
      : ofstream_(filename, std::ios::binary), descriptor_{archive_type,
                                                           compression} {

    oarchive_ = std::make_unique<boost::archive::binary_oarchive>(ofstream_);

    *oarchive_ << descriptor_;

    if (compression != ArchiveCompression::None) {
      out_ = std::make_unique<boost::iostreams::filtering_ostream>();
      if (compression == ArchiveCompression::Zstd) {
        out_->push(boost::iostreams::zstd_compressor());
      } else {
        throw std::runtime_error(
            "Unsupported compression type for output archive file \"" +
            filename + "\"");
      }
      out_->push(ofstream_);
      oarchive_ = std::make_unique<boost::archive::binary_oarchive>(
          *out_, boost::archive::no_header);
    }
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
  std::unique_ptr<boost::iostreams::filtering_ostream> out_;
  std::unique_ptr<boost::archive::binary_oarchive> oarchive_;
  ArchiveDescriptor descriptor_;

  void do_put(const Derived& item) { *oarchive_ << item; }
  // TODO(Jan): Solve this without the additional alloc/copy operation
};

} // namespace fles
