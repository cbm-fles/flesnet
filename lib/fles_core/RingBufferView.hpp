// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <cstdint>

/// Simple generic ring buffer view class.
template <typename T = uint8_t> class RingBufferView
{
public:
    /// The RingBufferView constructor.
    RingBufferView(T* buffer, std::size_t new_size_exponent)
        : _buf(buffer), _size_exponent(new_size_exponent),
          _size(UINT64_C(1) << _size_exponent),
          _size_mask((UINT64_C(1) << _size_exponent) - 1)
    {
    }

    /// The element accessor operator.
    T& at(std::size_t n) { return _buf[n & _size_mask]; }

    /// The const element accessor operator.
    const T& at(std::size_t n) const { return _buf[n & _size_mask]; }

    /// Retrieve pointer to memory buffer.
    T* ptr() { return _buf; }

    /// Retrieve const pointer to memory buffer.
    const T* ptr() const { return _buf; }

    /// Retrieve buffer size in maximum number of entries.
    std::size_t size() const { return _size; }

    /// Retrieve buffer size in maximum number of entries as two's exponent.
    std::size_t size_exponent() const { return _size_exponent; }

    /// Retrieve buffer size bit mask.
    std::size_t size_mask() const { return _size_mask; }

    /// Retrieve buffer size in bytes.
    std::size_t bytes() const { return _size * sizeof(T); }

private:
    /// The data buffer.
    T* _buf;

    /// Buffer size given as two's exponent.
    const std::size_t _size_exponent;

    /// Buffer size (maximum number of entries).
    const std::size_t _size;

    /// Buffer addressing bit mask.
    const std::size_t _size_mask;
};
