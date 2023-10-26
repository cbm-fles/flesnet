// Copyright 2016 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::OutputArchiveSequence template class.
#pragma once

#include "ArchiveDescriptor.hpp"
#include "Sink.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/filter/zstd.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace fles {

/**
 * \brief The OutputArchiveSequence class serializes data sets to a sequence of
 * output files.
 */
template <class Base, class Derived, ArchiveType archive_type>
class OutputArchiveSequence : public Sink<Base> {
public:
  /**
   * \brief Construct an output archive object, open the first archive file
   * for writing, and write the archive descriptor. All occurences of the
   * placeholder "%n" in the filename_template are replaced by a sequence
   * number.
   *
   * \param filename_template File name pattern of the archive files
   * \param items_per_file    Number of items to store in each file
   */
  explicit OutputArchiveSequence(
      std::string filename_template,
      std::size_t items_per_file = SIZE_MAX,
      std::size_t bytes_per_file = SIZE_MAX,
      ArchiveCompression compression = ArchiveCompression::None)
      : descriptor_{archive_type, compression},
        filename_template_(std::move(filename_template)),
        items_per_file_(items_per_file), bytes_per_file_(bytes_per_file) {
    if (items_per_file_ == 0) {
      items_per_file_ = SIZE_MAX;
    }
    if (bytes_per_file_ == 0) {
      bytes_per_file_ = SIZE_MAX;
    }

    // append sequence number to file name if missing in template
    if (items_per_file_ < SIZE_MAX &&
        filename_template_.find("%n") == std::string::npos) {
      filename_template_ += ".%n";
    }

    next_file();
  }

  /// Delete copy constructor (non-copyable).
  OutputArchiveSequence(const OutputArchiveSequence&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const OutputArchiveSequence&) = delete;

  ~OutputArchiveSequence() override = default;

  /// Store an item.
  void put(std::shared_ptr<const Base> item) override { do_put(*item); }

  void end_stream() override {
    oarchive_ = nullptr;
    out_ = nullptr;
    ofstream_ = nullptr;
  }

private:
  std::unique_ptr<std::ofstream> ofstream_;
  std::unique_ptr<boost::iostreams::filtering_ostream> out_;
  std::unique_ptr<boost::archive::binary_oarchive> oarchive_;
  ArchiveDescriptor descriptor_;

  std::string filename_template_;
  std::size_t items_per_file_;
  std::size_t bytes_per_file_;
  std::size_t file_count_ = 0;
  std::size_t file_item_count_ = 0;

  // TODO(Jan): Solve this without the additional alloc/copy operation
  void do_put(const Derived& item) {
    if (file_limit_reached()) {
      next_file();
    }
    *oarchive_ << item;
    ++file_item_count_;
  }

  [[nodiscard]] std::string filename(std::size_t n) const {
    std::ostringstream number;
    number << std::setw(4) << std::setfill('0') << n;
    return boost::replace_all_copy(filename_template_, "%n", number.str());
  }

  bool file_limit_reached() {
    // check item limit
    if (file_item_count_ == items_per_file_) {
      return true;
    }
    // check byte limit if set
    if (bytes_per_file_ < SIZE_MAX) {
      auto pos = ofstream_->tellp();
      if (pos > 0 && static_cast<std::size_t>(pos) >= bytes_per_file_) {
        return true;
      }
    }
    return false;
  }

  void next_file() {
    oarchive_ = nullptr;
    out_ = nullptr;
    ofstream_ = nullptr;
    ofstream_ = std::make_unique<std::ofstream>(filename(file_count_),
                                                std::ios::binary);
    oarchive_ = std::make_unique<boost::archive::binary_oarchive>(*ofstream_);
    *oarchive_ << descriptor_;

    if (descriptor_.archive_compression() != ArchiveCompression::None) {
      out_ = std::make_unique<boost::iostreams::filtering_ostream>();
      if (descriptor_.archive_compression() == ArchiveCompression::Zstd) {
        out_->push(boost::iostreams::zstd_compressor(
            boost::iostreams::zstd::best_speed));
      } else {
        throw std::runtime_error(
            "Unsupported compression type for output archive file \"" +
            filename(file_count_) + "\"");
      }
      out_->push(*ofstream_);
      oarchive_ = std::make_unique<boost::archive::binary_oarchive>(
          *out_, boost::archive::no_header);
    }

    ++file_count_;
    file_item_count_ = 0;
  }
};

} // namespace fles
