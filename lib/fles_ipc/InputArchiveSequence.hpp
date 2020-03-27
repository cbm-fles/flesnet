// Copyright 2013, 2020 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::InputArchiveSequence template class.
#pragma once

#include "ArchiveDescriptor.hpp"
#include "Source.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

namespace fles {

/**
 * \brief The InputArchiveSequence class deserializes microslice data sets from
 * a sequence of input files.
 */
template <class Base, class Derived, ArchiveType archive_type>
class InputArchiveSequence : public Source<Base> {
public:
  /**
   * \brief Construct an input archive object, open the first archive file for
   * reading, and read the archive descriptor. All occurences of the
   * placeholder "%n" in the filename_template are replaced by a sequence
   * number.
   *
   * \param filename_template File name pattern of the archive files
   */
  InputArchiveSequence(const std::string& filename_template)
      : filename_template_(filename_template) {
    // append sequence number to file name if missing in template
    if (filename_template_.find("%n") == std::string::npos) {
      filename_template_ += ".%n";
    }

    next_file();
  }

  /**
   * \brief Construct an input archive object, open the first archive file for
   * reading, and read the archive descriptor.
   *
   * \param filenames File names of the archive files
   */
  InputArchiveSequence(const std::vector<std::string>& filenames)
      : filenames_(filenames) {
    next_file();
  }

  /// Delete copy constructor (non-copyable).
  InputArchiveSequence(const InputArchiveSequence&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const InputArchiveSequence&) = delete;

  ~InputArchiveSequence() override = default;

  /// Read the next data set.
  std::unique_ptr<Derived> get() { return std::unique_ptr<Derived>(do_get()); };

  /// Retrieve the archive descriptor.
  const ArchiveDescriptor& descriptor() const { return descriptor_; };

  bool eos() const override { return eos_; }

private:
  std::unique_ptr<std::ifstream> ifstream_;
  std::unique_ptr<boost::archive::binary_iarchive> iarchive_;
  ArchiveDescriptor descriptor_;

  std::string filename_template_;
  const std::vector<std::string> filenames_;
  std::size_t file_count_ = 0;

  bool eos_ = false;

  std::string filename_with_number(std::size_t n) const {
    std::ostringstream number;
    number << std::setw(4) << std::setfill('0') << n;
    return boost::replace_all_copy(filename_template_, "%n", number.str());
  }

  void next_file() {
    iarchive_ = nullptr;
    ifstream_ = nullptr;

    std::string filename;
    if (!filenames_.empty()) {
      // We have a predefined vector
      if (file_count_ >= filenames_.size()) {
        eos_ = true;
        return;
      }
      filename = filenames_.at(file_count_);
    } else {
      // We count ourselves
      filename = filename_with_number(file_count_);
    }

    ifstream_ = std::unique_ptr<std::ifstream>(
        new std::ifstream(filename, std::ios::binary));
    if (!*ifstream_) {
      if (file_count_ == 0 || !filenames_.empty()) {
        // Not finding the first file or one given explicitely is an error
        throw std::ios_base::failure("error opening file \"" + filename + "\"");
      }
      // Not finding a later file is just the end-of-stream condition
      eos_ = true;
      return;
    }

    iarchive_ = std::unique_ptr<boost::archive::binary_iarchive>(
        new boost::archive::binary_iarchive(*ifstream_));

    *iarchive_ >> descriptor_;

    if (descriptor_.archive_type() != archive_type) {
      throw std::runtime_error("File \"" + filename +
                               "\" is not of correct archive type");
    }

    ++file_count_;
  }

  Derived* do_get() override {
    if (eos_) {
      return nullptr;
    }

    Derived* sts = nullptr;
    try {
      sts = new Derived();
      *iarchive_ >> *sts;
    } catch (boost::archive::archive_exception& e) {
      if (e.code == boost::archive::archive_exception::input_stream_error) {
        delete sts;
        next_file();
        if (!eos_) {
          return do_get();
        }
        return nullptr;
      }
      throw;
    }
    return sts;
  }
};

} // namespace fles
