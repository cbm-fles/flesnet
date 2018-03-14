// Copyright 2013, 2015, 2016 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::InputArchiveLoop template class.
#pragma once

#include "ArchiveDescriptor.hpp"
#include "Source.hpp"
#include <boost/archive/binary_iarchive.hpp>
#include <fstream>
#include <memory>
#include <string>

namespace fles {

/**
 * \brief The InputArchiveLoop class deserializes microslice data sets from an
 * input file. For testing, it can loop over the file a given number of times.
 */
template <class Base, class Derived, ArchiveType archive_type>
class InputArchiveLoop : public Source<Base> {
public:
  /**
   * \brief Construct an input archive object, open the given archive file for
   * reading, and read the archive descriptor.
   *
   * \param filename File name of the archive file
   * \param cycles   Number of times to loop over the archive file for
   */
  InputArchiveLoop(const std::string& filename, uint64_t cycles = 1)
      : filename_(filename), cycles_(cycles) {
    init();
  }

  /// Delete copy constructor (non-copyable).
  InputArchiveLoop(const InputArchiveLoop&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const InputArchiveLoop&) = delete;

  ~InputArchiveLoop() override = default;

  /// Read the next data set.
  std::unique_ptr<Derived> get() { return std::unique_ptr<Derived>(do_get()); };

  /// Retrieve the archive descriptor.
  const ArchiveDescriptor& descriptor() const { return descriptor_; };

  bool eos() const override { return eos_; }

private:
  void init() {
    iarchive_ = nullptr;
    ifstream_ = nullptr;

    ifstream_ = std::unique_ptr<std::ifstream>(
        new std::ifstream(filename_.c_str(), std::ios::binary));
    if (!*ifstream_) {
      throw std::ios_base::failure("error opening file \"" + filename_ + "\"");
    }

    iarchive_ = std::unique_ptr<boost::archive::binary_iarchive>(
        new boost::archive::binary_iarchive(*ifstream_));

    *iarchive_ >> descriptor_;

    if (descriptor_.archive_type() != archive_type) {
      throw std::runtime_error("File \"" + filename_ +
                               "\" is not of correct archive type");
    }

    ++cycle_;
    archive_has_data_ = false;
  }

  Derived* do_get() override {
    if (eos_) {
      return nullptr;
    }

    Derived* sts = nullptr;
    try {
      sts = new Derived();
      *iarchive_ >> *sts;
      archive_has_data_ = true;
    } catch (boost::archive::archive_exception& e) {
      if (e.code == boost::archive::archive_exception::input_stream_error) {
        delete sts;
        if (archive_has_data_ && cycle_ < cycles_) {
          init();
          return do_get();
        }
        eos_ = true;
        return nullptr;
      }
      throw;
    }
    return sts;
  }

  std::unique_ptr<std::ifstream> ifstream_;
  std::unique_ptr<boost::archive::binary_iarchive> iarchive_;
  ArchiveDescriptor descriptor_;

  std::string filename_;
  uint64_t cycles_;

  uint64_t cycle_ = 0;
  bool archive_has_data_ = false;
  bool eos_ = false;
};

} // namespace fles
