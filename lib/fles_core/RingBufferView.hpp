// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <boost/iterator/iterator_facade.hpp>
#include <cstddef>
#include <cstdint>
#include <type_traits>

/// Simple generic ring buffer iterator class.
template <typename T, bool POWER_OF_TWO = true>
class RingBufferIterator
    : public boost::iterator_facade<RingBufferIterator<T>,
                                    T,
                                    boost::random_access_traversal_tag> {
public:
  /// The RingBufferIterator constructor.
  RingBufferIterator(T* buffer, std::size_t size, std::size_t n)
      : buf_(buffer), size_(size), n_(n) {}
  // Need to add a default constructor for the iterator
  RingBufferIterator() : buf_(nullptr), size_(0), n_(0) {}

  [[nodiscard]] std::size_t get_index() const { return n_; }

private:
  friend class boost::iterator_core_access;

  /// The iterator dereference operator.
  T& dereference() const {
    if (POWER_OF_TWO) {
      return buf_[n_ & (size_ - 1)];
    }
    return buf_[n_ % size_];
  }

  /// The iterator equality operator.
  bool equal(RingBufferIterator const& other) const { return n_ == other.n_; }

  /// The iterator increment operator.
  void increment() { ++n_; }

  /// The iterator decrement operator.
  void decrement() { --n_; }

  /// The iterator addition operator.
  void advance(std::ptrdiff_t n) { n_ += n; }

  /// The iterator difference operator.
  std::ptrdiff_t distance_to(RingBufferIterator const& other) const {
    return other.n_ - n_;
  }

  T* buf_;
  std::size_t size_;
  std::size_t n_;
};

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

  /// Get interator to element.
  using T_iter = RingBufferIterator<T, POWER_OF_TWO>;
  [[nodiscard]]
  T_iter get_iter(std::size_t n) {
    return T_iter(buf_, size_, n);
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

  /// Retrieve the offset of the element at index n in bytes.
  [[nodiscard]] std::size_t offset_bytes(std::size_t n) const {
    return (POWER_OF_TWO ? (n & size_mask_) : (n % size_)) * sizeof(T);
  }

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
