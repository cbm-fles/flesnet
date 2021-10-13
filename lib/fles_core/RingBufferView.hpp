// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <cstddef>
#include <cstdint>

/// Simple generic ring buffer view class.
template <typename T> class RingBufferView {
public:
  /// The RingBufferView constructor.
  RingBufferView(T* buffer, std::size_t new_size_exponent)
      : buf_(buffer), size_exponent_(new_size_exponent),
        size_(UINT64_C(1) << size_exponent_),
        size_mask_((UINT64_C(1) << size_exponent_) - 1) {}

  /// The element accessor operator.
  T& at(std::size_t n) { return buf_[n & size_mask_]; }

  /// The const element accessor operator.
  [[nodiscard]] const T& at(std::size_t n) const {
    return buf_[n & size_mask_];
  }

  /// Retrieve pointer to memory buffer.
  T* ptr() { return buf_; }

  /// Retrieve const pointer to memory buffer.
  [[nodiscard]] const T* ptr() const { return buf_; }

  /// Retrieve buffer size in maximum number of entries.
  [[nodiscard]] std::size_t size() const { return size_; }

  /// Retrieve buffer size in maximum number of entries as two's exponent.
  [[nodiscard]] std::size_t size_exponent() const { return size_exponent_; }

  /// Retrieve buffer size bit mask.
  [[nodiscard]] std::size_t size_mask() const { return size_mask_; }

  /// Retrieve buffer size in bytes.
  [[nodiscard]] std::size_t bytes() const { return size_ * sizeof(T); }

private:
  /// The data buffer.
  T* buf_;

  /// Buffer size given as two's exponent.
  const std::size_t size_exponent_;

  /// Buffer size (maximum number of entries).
  const std::size_t size_;

  /// Buffer addressing bit mask.
  const std::size_t size_mask_;
};
