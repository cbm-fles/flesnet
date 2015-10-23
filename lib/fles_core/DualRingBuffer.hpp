// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "RingBufferView.hpp"
#include "MicrosliceDescriptor.hpp"

struct DualIndex {
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

DualIndex operator+(DualIndex, const DualIndex&);
DualIndex operator-(DualIndex, const DualIndex&);

bool operator<(const DualIndex&, const DualIndex&);
bool operator>(const DualIndex&, const DualIndex&);

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

/// Abstract FLES data source class.
template <typename T_DESC, typename T_DATA> class DualRingBufferReadInterface
{
public:
    virtual ~DualRingBufferReadInterface() {}

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
    virtual ~DualRingBufferWriteInterface() {}

    virtual DualIndex get_read_index() = 0;

    virtual void set_write_index(DualIndex new_write_index) = 0;

    virtual RingBufferView<T_DATA>& data_buffer() = 0;
    virtual RingBufferView<T_DESC>& desc_buffer() = 0;
};

using InputBufferReadInterface =
    DualRingBufferReadInterface<fles::MicrosliceDescriptor, uint8_t>;

using InputBufferWriteInterface =
    DualRingBufferWriteInterface<fles::MicrosliceDescriptor, uint8_t>;
