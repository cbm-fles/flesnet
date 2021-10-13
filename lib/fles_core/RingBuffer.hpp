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
template <typename T, bool CLEARED = false, bool PAGE_ALIGNED = false>
class RingBuffer {
public:
  void array_delete_(T* ptr, size_t size) const {
    while (size != 0u) {
      ptr[--size].~T();
    }
    free(const_cast<typename std::remove_volatile<T>::type*>(ptr));
  }

  using buf_t = std::unique_ptr<T[], std::function<void(T*)>>;

  /// The RingBuffer default constructor.
  RingBuffer() = default;

  /// The RingBuffer initializing constructor.
  explicit RingBuffer(size_t new_size_exponent) {
    alloc_with_size_exponent(new_size_exponent);
  }

  RingBuffer(const RingBuffer&) = delete;
  void operator=(const RingBuffer&) = delete;

  /// Create and initialize buffer with given minimum size.
  void alloc_with_size(size_t minimum_size) {
    size_t new_size_exponent = 0;
    if (minimum_size > 1) {
      minimum_size--;
      ++new_size_exponent;
      while ((minimum_size >>= 1) != 0u) {
        ++new_size_exponent;
      }
    }
    alloc_with_size_exponent(new_size_exponent);
  }

  /// Create and initialize buffer with given size exponent.
  void alloc_with_size_exponent(size_t new_size_exponent) {
    size_exponent_ = new_size_exponent;
    size_ = UINT64_C(1) << size_exponent_;
    size_mask_ = size_ - 1;
    if (PAGE_ALIGNED) {
      void* buf;
      const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
      int ret = posix_memalign(&buf, page_size, sizeof(T) * size_);
      if (ret != 0) {
        throw std::runtime_error(std::string("posix_memalign: ") +
                                 strerror(ret));
      }
      if (CLEARED) {
        // TODO(Jan): Remove this hack for gcc < 4.8
        // buf_ = buf_t(new(buf) T[size_](), [&](T* ptr){
        // array_delete_(ptr, size_); });
        buf_ = buf_t(new (buf) T[size_](), [&](T* ptr) {
          size_t size = size_;
          while (size != 0u) {
            ptr[--size].~T();
          }
          free(const_cast<typename std::remove_volatile<T>::type*>(ptr));
        });
      } else {
        buf_ = buf_t(new (buf) T[size_],
                     [&](T* ptr) { array_delete_(ptr, size_); });
      }
    } else {
      if (CLEARED) {
        buf_ = buf_t(new T[size_](), [&](T* ptr) { delete[] ptr; });
      } else {
        buf_ = buf_t(new T[size_], [&](T* ptr) { delete[] ptr; });
      }
    }
  }

  /// The element accessor operator.
  T& at(size_t n) { return buf_[n & size_mask_]; }

  /// The const element accessor operator.
  [[nodiscard]] const T& at(size_t n) const { return buf_[n & size_mask_]; }

  /// Retrieve pointer to memory buffer.
  T* ptr() { return buf_.get(); }

  /// Retrieve const pointer to memory buffer.
  [[nodiscard]] const T* ptr() const { return buf_.get(); }

  /// Retrieve buffer size in maximum number of entries.
  [[nodiscard]] size_t size() const { return size_; }

  /// Retrieve buffer size in maximum number of entries as two's exponent.
  [[nodiscard]] size_t size_exponent() const { return size_exponent_; }

  /// Retrieve buffer size bit mask.
  [[nodiscard]] size_t size_mask() const { return size_mask_; }

  /// Retrieve buffer size in bytes.
  [[nodiscard]] size_t bytes() const { return size_ * sizeof(T); }

  void clear() { std::fill_n(buf_, size_, T()); }

private:
  /// Buffer size (maximum number of entries).
  size_t size_ = 0;

  /// Buffer size given as two's exponent.
  size_t size_exponent_ = 0;

  /// Buffer addressing bit mask.
  size_t size_mask_ = 0;

  /// The data buffer.
  buf_t buf_;
};
