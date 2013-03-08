/**
 * \file RingutBuffer.hpp
 *
 * 2012, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef RINGBUFFER_HPP
#define RINGBUFFER_HPP


/// Simple generic ring buffer class.
template<typename T>
class RingBuffer
{
public:
    /// The RingBuffer default constructor.
    RingBuffer() { }
    
    /// The RingBuffer initializing constructor.
    RingBuffer(size_t size_exponent) {
        alloc_with_size_exponent(size_exponent);
    }

    /// The RingBuffer destructor.
    ~RingBuffer() {
        delete [] _buf;
    }

    /// Create and initialize buffer with given minimum size.
    void alloc_with_size(size_t minimum_size) {
        int size_exponent = 0;
        if (minimum_size > 1) {
            minimum_size--;
            size_exponent++;
            while (minimum_size >>= 1)
                size_exponent++;
        }
        alloc_with_size_exponent(size_exponent);
    }

    /// Create and initialize buffer with given size exponent.
    void alloc_with_size_exponent(size_t size_exponent) {
        _size_exponent = size_exponent;
        _size_mask = (1 << size_exponent) - 1;
        _buf = new T[1 << _size_exponent]();
    }
    
    /// The element accessor operator.
    T& at(size_t n) {
        return _buf[n & _size_mask];
    }

    /// The const element accessor operator.
    const T& at(size_t n) const {
        return _buf[n & _size_mask];
    }

    /// Retrieve pointer to memory buffer.
    T* ptr() {
        return _buf;
    }

    /// Retrieve const pointer to memory buffer.
    const T* ptr() const {
        return _buf;
    }

    /// Retrieve buffer size in maximum number of entries.
    size_t size() const {
        return (1 << _size_exponent);
    }

    /// Retrieve buffer size bit mask.
    size_t size_mask() const {
        return _size_mask;
    }

    /// Retrieve buffer size in bytes.
    size_t bytes() const {
        return (1 << _size_exponent) * sizeof(T);
    }

private:
    /// Buffer size given as two's exponent.
    size_t _size_exponent = 0;

    /// Buffer addressing bit mask
    size_t _size_mask;
    
    /// The data buffer.
    T* _buf = nullptr;
};


#endif /* RINGBUFFER_HPP */
