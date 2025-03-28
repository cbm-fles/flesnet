// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unistd.h>

/// Simple generic ring buffer class.
template <typename T,
          bool CLEARED = false,
          bool PAGE_ALIGNED = false,
          bool POWER_OF_TWO = true>
class RingBuffer {
public:
  void array_delete_(T* ptr, size_t size) const {
    while (size != 0u) {
      ptr[--size].~T();
    }
    // NOLINTNEXTLINE
    free(const_cast<typename std::remove_volatile<T>::type*>(ptr));
  }

  using buf_t = std::unique_ptr<T[], std::function<void(T*)>>;

  /// The RingBuffer default constructor.
  RingBuffer() = default;

  /// The RingBuffer initializing constructor.
  explicit RingBuffer(size_t new_size) {
    if (POWER_OF_TWO) {
      alloc_with_size_exponent(new_size);
    } else {
      alloc_with_size(new_size);
    }
  }

  RingBuffer(const RingBuffer&) = delete;
  void operator=(const RingBuffer&) = delete;

  /// Create and initialize buffer with given size.
  void alloc_with_size(size_t size) {
    if (POWER_OF_TWO) {
      // Interpret size as minimum size, round up to next power of two.
      size_t new_size_exponent = size_to_exponent(size);
      alloc_with_size_exponent(new_size_exponent);
    } else {
      // Interpret size as exact size.
      size_ = size;
      do_alloc();
    }
  }

  /// Create and initialize buffer with given size exponent.
  void alloc_with_size_exponent(size_t new_size_exponent) {
    size_exponent_ = new_size_exponent;
    size_ = UINT64_C(1) << size_exponent_;
    size_mask_ = size_ - 1;
    do_alloc();
  }

  /// The element accessor operator.
  T& at(size_t n) {
    if (POWER_OF_TWO) {
      return buf_[n & size_mask_];
    }
    return buf_[n % size_];
  }

  /// The const element accessor operator.
  [[nodiscard]] const T& at(size_t n) const {
    if (POWER_OF_TWO) {
      return buf_[n & size_mask_];
    }
    return buf_[n % size_];
  }

  /// Retrieve pointer to memory buffer.
  T* ptr() { return buf_.get(); }

  /// Retrieve const pointer to memory buffer.
  [[nodiscard]] const T* ptr() const { return buf_.get(); }

  /// Retrieve buffer size in maximum number of entries.
  [[nodiscard]] size_t size() const { return size_; }

  /// Retrieve buffer size in maximum number of entries as two's exponent.
  /// Only available if POWER_OF_TWO is true.
  template <bool P = POWER_OF_TWO>
  [[nodiscard]] typename std::enable_if<P, size_t>::type size_exponent() const {
    return size_exponent_;
  }

  /// Retrieve buffer size bit mask.
  /// Only available if POWER_OF_TWO is true.
  template <bool P = POWER_OF_TWO>
  [[nodiscard]] typename std::enable_if<P, size_t>::type size_mask() const {
    return size_mask_;
  }

  /// Retrieve buffer size in bytes.
  [[nodiscard]] size_t bytes() const { return size_ * sizeof(T); }

  void clear() { std::fill_n(buf_, size_, T()); }

private:
  /// Convert size to two's exponent.
  static size_t size_to_exponent(size_t size) {
    size_t exp = 0;
    while ((UINT64_C(1) << exp) < size) {
      ++exp;
    }
    return exp;
  }

  void do_alloc() {
    if (size_ == 0) {
      throw std::runtime_error("RingBuffer: size is zero");
    }
    if (PAGE_ALIGNED) {
      void* buf;
      const auto page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
      int ret = posix_memalign(&buf, page_size, sizeof(T) * size_);
      if (ret != 0) {
        throw std::runtime_error(std::string("posix_memalign: ") +
                                 strerror(ret));
      }
      if (CLEARED) {
        buf_ = buf_t(new (buf) T[size_](),
                     [&](T* ptr) { array_delete_(ptr, size_); });
      } else {
        buf_ = buf_t(new (buf) T[size_],
                     [&](T* ptr) { array_delete_(ptr, size_); });
      }
    } else {
      if (CLEARED) {
        // NOLINTNEXTLINE
        buf_ = buf_t(new T[size_](), [&](T* ptr) { delete[] ptr; });
      } else {
        // NOLINTNEXTLINE
        buf_ = buf_t(new T[size_], [&](T* ptr) { delete[] ptr; });
      }
    }
  }

  /// Buffer size (maximum number of entries).
  size_t size_ = 0;

  /// Buffer size given as two's exponent.
  size_t size_exponent_ = 0;

  /// Buffer addressing bit mask (only used when POWER_OF_TWO is true).
  size_t size_mask_ = 0;

  /// The data buffer.
  buf_t buf_;
};
