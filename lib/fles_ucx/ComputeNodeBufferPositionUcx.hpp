// Copyright 2012-2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <cstdint>

#pragma pack(1)

/// Structure representing a set of compute node buffer positions.
struct ComputeNodeBufferPositionUcx {
  uint64_t data; ///< The position in the data buffer.
  uint64_t desc; ///< The position in the descriptor buffer.
  bool operator<(const ComputeNodeBufferPositionUcx& rhs) const {
    return desc < rhs.desc;
  }
  bool operator>(const ComputeNodeBufferPositionUcx& rhs) const {
    return desc > rhs.desc;
  }
  bool operator==(const ComputeNodeBufferPositionUcx& rhs) const {
    return desc == rhs.desc && data == rhs.data;
  }
  bool operator!=(const ComputeNodeBufferPositionUcx& rhs) const {
    return desc != rhs.desc || data != rhs.data;
  }
};

#pragma pack()
