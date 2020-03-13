// Copyright 2016 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "RingBufferView.hpp"
#include <algorithm>
#include <cassert>

/// Simple generic managed ring buffer class.
template <typename T> class ManagedRingBuffer : public RingBufferView<T> {
public:
  /// The ManagedRingBuffer constructor.
  ManagedRingBuffer(T* buffer, std::size_t new_size_exponent)
      : RingBufferView<T>(buffer, new_size_exponent) {}

  std::size_t write_index() const { return write_index_; }

  std::size_t read_index() const { return read_index_; }

  void set_read_index(std::size_t new_index) { read_index_ = new_index; }

  std::size_t size_used() const {
    assert(write_index_ >= read_index_);
    return write_index_ - read_index_;
  }

  std::size_t size_available() const {
    assert(this->size() > size_used());
    return this->size() - size_used();
  }

  std::size_t size_available_contiguous() const {
    std::size_t offset = write_index_ & (this->size() - 1);
    std::size_t size_to_border = this->size() - offset;

    if (size_available() > size_to_border) {
      return std::max(size_to_border, size_available() - size_to_border);
    }
    return size_available();
  }

  void append(const T& item) {
    assert(size_available() >= 1);
    this->at(write_index_) = item;
    ++write_index_;
  }

  void append(const T* buf, std::size_t n) {
    if (&this->at(write_index_) + n <= this->ptr() + this->size()) {
      // one chunk
      std::copy(buf, buf + n, &this->at(write_index_));
    } else {
      // two chunks
      size_t a = this->ptr() + this->size() - &this->at(write_index_);
      std::copy(buf, buf + a, &this->at(write_index_));
      std::copy(buf + a, buf + n, &this->at(write_index_ + a));
    }
    write_index_ += n;
  }

  // skip remaining entries in ring buffer so that n entries can be stored
  // without fragmentation
  void skip_buffer_wrap(std::size_t n) {
    assert(size_available() >= n);
    std::size_t offset = write_index_ & (this->size() - 1);

    if (offset + n > this->size()) {
      write_index_ += this->size() - offset;
    }
  }

private:
  std::size_t write_index_ = 0;

  std::size_t read_index_ = 0;
};
