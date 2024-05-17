// Copyright 2013, 2015, 2016 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::InputArchiveLoop template class.
#pragma once

#include "ArchiveDescriptor.hpp"
#include "Source.hpp"
#include <boost/archive/binary_iarchive.hpp>
#ifdef BOOST_IOS_HAS_ZSTD
#include <boost/iostreams/filter/zstd.hpp>
#endif
#include <boost/iostreams/filtering_stream.hpp>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

namespace fles {

/**
 * \brief The InputArchiveLoop class deserializes data sets from an input file.
 * For testing, it can loop over the file a given number of times.
 */
template <class Base, class Storable, ArchiveType archive_type>
class InputArchiveLoop : public Source<Base> {
public:
  /**
   * \brief Construct an input archive object, open the given archive file for
   * reading, and read the archive descriptor.
   *
   * \param filename File name of the archive file
   * \param cycles   Number of times to loop over the archive file for
   */
  InputArchiveLoop(std::string filename, uint64_t cycles = 1)
      : filename_(std::move(filename)), cycles_(cycles) {
    init();
  }

  /// Delete copy constructor (non-copyable).
  InputArchiveLoop(const InputArchiveLoop&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const InputArchiveLoop&) = delete;

  ~InputArchiveLoop() override = default;

  /// Read the next data set.
  std::unique_ptr<Storable> get() {
    return std::unique_ptr<Storable>(do_get());
  };

  /// Retrieve the archive descriptor.
  [[nodiscard]] const ArchiveDescriptor& descriptor() const {
    return descriptor_;
  };

  [[nodiscard]] bool eos() const override { return eos_; }

private:
  void init() {
    iarchive_ = nullptr;
    ifstream_ = nullptr;

    ifstream_ =
        std::make_unique<std::ifstream>(filename_.c_str(), std::ios::binary);
    if (!*ifstream_) {
      throw std::ios_base::failure("error opening file \"" + filename_ + "\"");
    }

    iarchive_ = std::make_unique<boost::archive::binary_iarchive>(*ifstream_);

    *iarchive_ >> descriptor_;

    if (descriptor_.archive_type() != archive_type) {
      throw std::runtime_error("File \"" + filename_ +
                               "\" is not of correct archive type");
    }

    if (descriptor_.archive_compression() != ArchiveCompression::None) {
#ifdef BOOST_IOS_HAS_ZSTD
      in_ = std::make_unique<boost::iostreams::filtering_istream>();
      if (descriptor_.archive_compression() == ArchiveCompression::Zstd) {
        in_->push(boost::iostreams::zstd_decompressor());
      } else {
        throw std::runtime_error(
            "Unsupported compression type for input archive file \"" +
            filename_ + "\"");
      }
      in_->push(*ifstream_);
      iarchive_ = std::make_unique<boost::archive::binary_iarchive>(
          *in_, boost::archive::no_header);
#else
      throw std::runtime_error(
          "Unsupported compression type for input archive file \"" + filename_ +
          "\"");
#endif
    }

    ++cycle_;
    archive_has_data_ = false;
  }

  Storable* do_get() override {
    if (eos_) {
      return nullptr;
    }

    Storable* sts = nullptr;
    try {
      sts = new Storable(); // NOLINT
      *iarchive_ >> *sts;
      archive_has_data_ = true;
    } catch (boost::archive::archive_exception& e) {
      if (e.code == boost::archive::archive_exception::input_stream_error) {
        delete sts; // NOLINT
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
  std::unique_ptr<boost::iostreams::filtering_istream> in_;
  std::unique_ptr<boost::archive::binary_iarchive> iarchive_;
  ArchiveDescriptor descriptor_;

  std::string filename_;
  uint64_t cycles_;

  uint64_t cycle_ = 0;
  bool archive_has_data_ = false;
  bool eos_ = false;
};

} // namespace fles
