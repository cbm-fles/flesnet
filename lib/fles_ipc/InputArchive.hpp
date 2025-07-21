// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::InputArchive template class.
#pragma once

#include "ArchiveDescriptor.hpp"
#include "BoostHelper.hpp"
#include "Source.hpp"
#include "log.hpp"
#include <boost/archive/archive_exception.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/basic_archive.hpp> // for error messages
#ifdef BOOST_IOS_HAS_ZSTD
  #include <boost/iostreams/filter/zstd.hpp>
#endif
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/version.hpp>
#include <fstream>
#include <memory>
#include <string>

namespace fles {

/**
 * \brief The InputArchive class deserializes data sets from an input file.
 */
template <class Base, class Storable, ArchiveType archive_type>
class InputArchive : public Source<Base> {
public:
  /**
   * \brief Construct an input archive object, open the given archive file for
   * reading, and read the archive descriptor.
   *
   * \param filename File name of the archive file
   */
  explicit InputArchive(const std::string& filename) {
    ifstream_ =
        std::make_unique<std::ifstream>(filename.c_str(), std::ios::binary);
    if (!*ifstream_) {
      throw std::ios_base::failure("error opening file \"" + filename + "\"");
    }

    try {
      iarchive_ = std::make_unique<boost::archive::binary_iarchive>(*ifstream_);
    } catch (boost::archive::archive_exception const &e) {
      switch (e.code) {
      case boost::archive::archive_exception::unsupported_version:
        L_(warning) << "This executable has support up to archive version "
                    << boost::archive::BOOST_ARCHIVE_VERSION() << std::endl;
        L_(warning) << "Consider recompiling with BOOST library >="
                    << boostlib_for_archive_version(
                      // future improvement: better query for the
                      // version found in the failed archive, but
                      // there seems no easy way to do that. So, we
                      // propose to use at least a boost library
                      // version supporting the next archive version.
                      boost::archive::BOOST_ARCHIVE_VERSION()+1)
                    << " (this uses boost " << BOOST_LIB_VERSION << ")." << std::endl;
        throw e;
        break;
      default:
        throw e;
      }
    }

    *iarchive_ >> descriptor_;

    if (descriptor_.archive_type() != archive_type) {
      throw std::runtime_error("File \"" + filename +
                               "\" is not of correct archive type. InputArchive expected \"" +
                               ArchiveTypeToString(archive_type) +
                               "\" found \"" +
                               ArchiveTypeToString(descriptor_.archive_type()) + "\"."
        );
    }

    if (descriptor_.archive_compression() != ArchiveCompression::None) {
#ifdef BOOST_IOS_HAS_ZSTD
      in_ = std::make_unique<boost::iostreams::filtering_istream>();
      if (descriptor_.archive_compression() == ArchiveCompression::Zstd) {
        in_->push(boost::iostreams::zstd_decompressor());
      } else {
        throw std::runtime_error(
            "Unsupported compression type for input archive file \"" +
            filename + "\". Expected " + ArchiveCompressionToString(ArchiveCompression::Zstd) + ".");
      }
      in_->push(*ifstream_);
      iarchive_ = std::make_unique<boost::archive::binary_iarchive>(
          *in_, boost::archive::no_header);
#else
      throw std::runtime_error(
          "Unsupported compression type for input archive file \"" +
          filename + "\"");
#endif
    }
  }

  /// Delete copy constructor (non-copyable).
  InputArchive(const InputArchive&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const InputArchive&) = delete;

  ~InputArchive() override = default;

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
  Storable* do_get() override {
    if (eos_) {
      return nullptr;
    }

    Storable* sts = nullptr;
    try {
      sts = new Storable(); // NOLINT
      *iarchive_ >> *sts;
    } catch (boost::archive::archive_exception& e) {
      if (e.code == boost::archive::archive_exception::input_stream_error) {
        delete sts; // NOLINT
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

  bool eos_ = false;
};

} // namespace fles
