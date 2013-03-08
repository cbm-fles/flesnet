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
    RingBuffer(size_t sizeExponent) {
        allocWithSizeExponent(sizeExponent);
    }

    /// The RingBuffer destructor.
    ~RingBuffer() {
        delete [] _buf;
    }

    /// Create and initialize buffer with given minimum size.
    void allocWithSize(size_t minimumSize) {
        int sizeExponent = 0;
        if (minimumSize > 1) {
            minimumSize--;
            sizeExponent++;
            while (minimumSize >>= 1)
                sizeExponent++;
        }
        allocWithSizeExponent(sizeExponent);
    }

    /// Create and initialize buffer with given size exponent.
    void allocWithSizeExponent(size_t sizeExponent) {
        _sizeExponent = sizeExponent;
        _sizeMask = (1 << sizeExponent) - 1;
        _buf = new T[1 << _sizeExponent]();
    }
    
    /// The element accessor operator.
    T& at(size_t n) {
        return _buf[n & _sizeMask];
    }

    /// The const element accessor operator.
    const T& at(size_t n) const {
        return _buf[n & _sizeMask];
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
        return (1 << _sizeExponent);
    }

    /// Retrieve buffer size bit mask.
    size_t sizeMask() const {
        return _sizeMask;
    }

    /// Retrieve buffer size in bytes.
    size_t bytes() const {
        return (1 << _sizeExponent) * sizeof(T);
    }

private:
    /// Buffer size given as two's exponent.
    size_t _sizeExponent = 0;

    /// Buffer addressing bit mask
    size_t _sizeMask;
    
    /// The data buffer.
    T* _buf = nullptr;
};


#endif /* RINGBUFFER_HPP */
