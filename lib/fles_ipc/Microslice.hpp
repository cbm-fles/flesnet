// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::Microslice abstract base class.
#pragma once

#include "MicrosliceDescriptor.hpp"
#include <boost/serialization/access.hpp>

namespace fles {

/**
 * \brief The Microslice class provides read access to the data of a microslice.
 *
 * This class is an abstract base class for all classes providing access to the
 * descriptor and data contents of a single microslice.
 */
class Microslice {
public:
  virtual ~Microslice() = 0;

  /// Retrieve microslice descriptor reference
  [[nodiscard]] const MicrosliceDescriptor& desc() const { return *desc_ptr_; }

  /// Retrieve a pointer to the microslice data
  [[nodiscard]] const uint8_t* content() const { return content_ptr_; }

  /// Compute CRC-32 checksum of microslice data content
  [[nodiscard]] uint32_t compute_crc() const;

  /// Compare computed CRC-32 checksum to value in header
  [[nodiscard]] bool check_crc() const;

protected:
  Microslice() = default;

  /// Construct microslice with given content.
  Microslice(MicrosliceDescriptor* desc_ptr, uint8_t* content_ptr)
      : desc_ptr_(desc_ptr), content_ptr_(content_ptr){};

  friend class StorableMicroslice;

  /// Pointer to the microslice descriptor
  MicrosliceDescriptor* desc_ptr_ = nullptr;

  /// Pointer to the microslice data content
  uint8_t* content_ptr_ = nullptr;
};

} // namespace fles
