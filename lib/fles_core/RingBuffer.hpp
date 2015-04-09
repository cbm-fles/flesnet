// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <memory>

/// Simple generic ring buffer class.
template <typename T = uint8_t, bool CLEARED = false> class RingBuffer
{
public:
    /// The RingBuffer default constructor.
    RingBuffer() {}

    /// The RingBuffer initializing constructor.
    explicit RingBuffer(size_t new_size_exponent)
    {
        alloc_with_size_exponent(new_size_exponent);
    }

    RingBuffer(const RingBuffer&) = delete;
    void operator=(const RingBuffer&) = delete;

    /// Create and initialize buffer with given minimum size.
    void alloc_with_size(size_t minimum_size)
    {
        size_t new_size_exponent = 0;
        if (minimum_size > 1) {
            minimum_size--;
            ++new_size_exponent;
            while (minimum_size >>= 1)
                ++new_size_exponent;
        }
        alloc_with_size_exponent(new_size_exponent);
    }

    /// Create and initialize buffer with given size exponent.
    void alloc_with_size_exponent(size_t new_size_exponent)
    {
        _size_exponent = new_size_exponent;
        _size = UINT64_C(1) << _size_exponent;
        _size_mask = _size - 1;
        if (CLEARED) {
            std::unique_ptr<T[]> buf(new T[_size]());
            _buf = std::move(buf);
        } else {
            std::unique_ptr<T[]> buf(new T[_size]);
            _buf = std::move(buf);
        }
    }

    /// The element accessor operator.
    T& at(size_t n) { return _buf[n & _size_mask]; }

    /// The const element accessor operator.
    const T& at(size_t n) const { return _buf[n & _size_mask]; }

    /// Retrieve pointer to memory buffer.
    T* ptr() { return _buf.get(); }

    /// Retrieve const pointer to memory buffer.
    const T* ptr() const { return _buf.get(); }

    /// Retrieve buffer size in maximum number of entries.
    size_t size() const { return _size; }

    /// Retrieve buffer size in maximum number of entries as two's exponent.
    size_t size_exponent() const { return _size_exponent; }

    /// Retrieve buffer size bit mask.
    size_t size_mask() const { return _size_mask; }

    /// Retrieve buffer size in bytes.
    size_t bytes() const { return _size * sizeof(T); }

    void clear() { std::fill_n(_buf, _size, T()); }

private:
    /// Buffer size (maximum number of entries).
    size_t _size = 0;

    /// Buffer size given as two's exponent.
    size_t _size_exponent = 0;

    /// Buffer addressing bit mask.
    size_t _size_mask = 0;

    /// The data buffer.
    std::unique_ptr<T[]> _buf;
};
