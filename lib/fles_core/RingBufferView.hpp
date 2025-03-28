// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

/// Simple generic ring buffer view class.
template <typename T, bool POWER_OF_TWO = true> class RingBufferView {
public:
  /// The RingBufferView constructor.
  RingBufferView(T* buffer, std::size_t new_size) : buf_(buffer) {
    if (POWER_OF_TWO) {
      size_exponent_ = new_size;
      size_ = UINT64_C(1) << size_exponent_;
      size_mask_ = size_ - 1;
    } else {
      size_ = new_size;
    }
  }

  /// The element accessor operator.
  T& at(std::size_t n) {
    if (POWER_OF_TWO) {
      return buf_[n & size_mask_];
    }
    return buf_[n % size_];
  }

  /// The const element accessor operator.
  [[nodiscard]] const T& at(std::size_t n) const {
    if (POWER_OF_TWO) {
      return buf_[n & size_mask_];
    }
    return buf_[n % size_];
  }

  /// Retrieve pointer to memory buffer.
  T* ptr() { return buf_; }

  /// Retrieve const pointer to memory buffer.
  [[nodiscard]] const T* ptr() const { return buf_; }

  /// Retrieve buffer size in maximum number of entries.
  [[nodiscard]] std::size_t size() const { return size_; }

  /// Retrieve buffer size in maximum number of entries as two's exponent.
  template <bool P = POWER_OF_TWO>
  [[nodiscard]] typename std::enable_if<P, std::size_t>::type
  size_exponent() const {
    return size_exponent_;
  }

  /// Retrieve buffer size bit mask.
  template <bool P = POWER_OF_TWO>
  [[nodiscard]] typename std::enable_if<P, std::size_t>::type
  size_mask() const {
    return size_mask_;
  }

  /// Retrieve buffer size in bytes.
  [[nodiscard]] std::size_t bytes() const { return size_ * sizeof(T); }

private:
  /// The data buffer.
  T* buf_;

  /// Buffer size given as two's exponent.
  std::size_t size_exponent_;

  /// Buffer size (maximum number of entries).
  std::size_t size_;

  /// Buffer addressing bit mask.
  std::size_t size_mask_;
};
