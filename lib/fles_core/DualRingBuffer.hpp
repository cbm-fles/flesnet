// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "MicrosliceDescriptor.hpp"
#include "RingBufferView.hpp"

struct DualIndex {
  uint64_t desc;
  uint64_t data;

  DualIndex& operator+=(const DualIndex& other) {
    desc += other.desc;
    data += other.data;

    return *this;
  };

  DualIndex& operator-=(const DualIndex& other) {
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

inline DualIndex operator+(DualIndex lhs, const DualIndex& rhs) {
  return lhs += rhs;
}

inline DualIndex operator-(DualIndex lhs, const DualIndex& rhs) {
  return lhs -= rhs;
}

inline bool operator<(const DualIndex& lhs, const DualIndex& rhs) {
  return lhs.desc < rhs.desc && lhs.data < rhs.data;
}

inline bool operator>(const DualIndex& lhs, const DualIndex& rhs) {
  return lhs.desc > rhs.desc && lhs.data > rhs.data;
}

inline bool operator<=(const DualIndex& lhs, const DualIndex& rhs) {
  return lhs.desc <= rhs.desc && lhs.data <= rhs.data;
}

inline bool operator>=(const DualIndex& lhs, const DualIndex& rhs) {
  return lhs.desc >= rhs.desc && lhs.data >= rhs.data;
}

inline bool operator==(const DualIndex& lhs, const DualIndex& rhs) {
  return lhs.desc == rhs.desc && lhs.data == rhs.data;
}

/// Abstract FLES data source class.
template <typename T_DESC, typename T_DATA, bool POWER_OF_TWO = true>
class DualRingBufferReadInterface {
public:
  virtual ~DualRingBufferReadInterface() = default;

  virtual void proceed() {}

  virtual DualIndex get_write_index() = 0;

  virtual bool get_eof() = 0;

  virtual void set_read_index(DualIndex new_read_index) = 0;
  virtual DualIndex get_read_index() = 0;

  virtual RingBufferView<T_DATA, POWER_OF_TWO>& data_buffer() = 0;
  virtual RingBufferView<T_DESC, POWER_OF_TWO>& desc_buffer() = 0;
};

template <typename T_DESC, typename T_DATA, bool POWER_OF_TWO = true>
class DualRingBufferWriteInterface {
public:
  virtual ~DualRingBufferWriteInterface() = default;

  virtual DualIndex get_read_index() = 0;

  virtual void set_write_index(DualIndex new_write_index) = 0;

  virtual void set_eof(bool eof) = 0;

  virtual RingBufferView<T_DATA, POWER_OF_TWO>& data_buffer() = 0;
  virtual RingBufferView<T_DESC, POWER_OF_TWO>& desc_buffer() = 0;
};

using InputBufferReadInterface =
    DualRingBufferReadInterface<fles::MicrosliceDescriptor, uint8_t>;

using InputBufferWriteInterface =
    DualRingBufferWriteInterface<fles::MicrosliceDescriptor, uint8_t>;
