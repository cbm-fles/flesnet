// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "MicrosliceDescriptor.hpp"
#include "RingBufferView.hpp"

#if defined(__GNUC__) && !defined(__clang__) &&                                \
    (__GNUC__ * 100 + __GNUC_MINOR__) < 501
// NOTE: Workaround for std::atomic bug in gcc versions < 5.1
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65147
struct alignas(16) DualIndex {
#else
struct DualIndex {
#endif
    uint64_t desc;
    uint64_t data;

    DualIndex& operator+=(const DualIndex& other)
    {
        desc += other.desc;
        data += other.data;

        return *this;
    };

    DualIndex& operator-=(const DualIndex& other)
    {
        desc -= other.desc;
        data -= other.data;

        return *this;
    };
};

DualIndex operator+(DualIndex /*lhs*/, const DualIndex& /*rhs*/);
DualIndex operator-(DualIndex /*lhs*/, const DualIndex& /*rhs*/);

bool operator<(const DualIndex& /*lhs*/, const DualIndex& /*rhs*/);
bool operator>(const DualIndex& /*lhs*/, const DualIndex& /*rhs*/);

bool operator<=(const DualIndex& /*lhs*/, const DualIndex& /*rhs*/);
bool operator>=(const DualIndex& /*lhs*/, const DualIndex& /*rhs*/);

bool operator==(const DualIndex& /*lhs*/, const DualIndex& /*rhs*/);

inline DualIndex operator+(DualIndex lhs, const DualIndex& rhs)
{
    return lhs += rhs;
}

inline DualIndex operator-(DualIndex lhs, const DualIndex& rhs)
{
    return lhs -= rhs;
}

inline bool operator<(const DualIndex& lhs, const DualIndex& rhs)
{
    return lhs.desc < rhs.desc && lhs.data < rhs.data;
}

inline bool operator>(const DualIndex& lhs, const DualIndex& rhs)
{
    return lhs.desc > rhs.desc && lhs.data > rhs.data;
}

inline bool operator<=(const DualIndex& lhs, const DualIndex& rhs)
{
    return lhs.desc <= rhs.desc && lhs.data <= rhs.data;
}

inline bool operator>=(const DualIndex& lhs, const DualIndex& rhs)
{
    return lhs.desc >= rhs.desc && lhs.data >= rhs.data;
}

inline bool operator==(const DualIndex& lhs, const DualIndex& rhs)
{
    return lhs.desc == rhs.desc && lhs.data == rhs.data;
}

/// Abstract FLES data source class.
template <typename T_DESC, typename T_DATA> class DualRingBufferReadInterface
{
public:
    virtual ~DualRingBufferReadInterface() = default;

    virtual void proceed() {}

    virtual DualIndex get_write_index() = 0;

    virtual void set_read_index(DualIndex new_read_index) = 0;

    virtual RingBufferView<T_DATA>& data_buffer() = 0;
    virtual RingBufferView<T_DESC>& desc_buffer() = 0;

    virtual RingBufferView<T_DATA>& data_send_buffer() { return data_buffer(); }
    virtual RingBufferView<T_DESC>& desc_send_buffer() { return desc_buffer(); }

    virtual void copy_to_data_send_buffer(std::size_t /* start */,
                                          std::size_t /* count */)
    {
    }

    virtual void copy_to_desc_send_buffer(std::size_t /* start */,
                                          std::size_t /* count */)
    {
    }
};

template <typename T_DESC, typename T_DATA> class DualRingBufferWriteInterface
{
public:
    virtual ~DualRingBufferWriteInterface() = default;

    virtual DualIndex get_read_index() = 0;

    virtual void set_write_index(DualIndex new_write_index) = 0;

    virtual RingBufferView<T_DATA>& data_buffer() = 0;
    virtual RingBufferView<T_DESC>& desc_buffer() = 0;
};

using InputBufferReadInterface =
    DualRingBufferReadInterface<fles::MicrosliceDescriptor, uint8_t>;

using InputBufferWriteInterface =
    DualRingBufferWriteInterface<fles::MicrosliceDescriptor, uint8_t>;
