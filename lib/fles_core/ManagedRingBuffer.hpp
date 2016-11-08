// Copyright 2016 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "RingBufferView.hpp"
#include <algorithm>
#include <cassert>

/// Simple generic managed ring buffer class.
template <typename T> class ManagedRingBuffer : public RingBufferView<T>
{
public:
    /// The ManagedRingBuffer constructor.
    ManagedRingBuffer(T* buffer, std::size_t new_size_exponent)
        : RingBufferView<T>(buffer, new_size_exponent)
    {
    }

    std::size_t write_index() const { return write_index_; }

    std::size_t read_index() const { return read_index_; }

    void set_read_index(std::size_t new_index) { read_index_ = new_index; }

    std::size_t size_used() const
    {
        assert(write_index_ >= read_index_);
        return write_index_ - read_index_;
    }

    std::size_t size_available() const
    {
        assert(this->size() > size_used());
        return this->size() - size_used();
    }

    void append(const T& item)
    {
        assert(size_available() >= 1);
        this->at(write_index_) = item;
        ++write_index_;
    }

    void append(T* buf, std::size_t n)
    {
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

private:
    std::size_t write_index_ = 0;

    std::size_t read_index_ = 0;
};
