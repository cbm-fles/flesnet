// Copyright 2012-2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <cstdint>

#pragma pack(1)

/// Access information for a remote memory region.
struct BufferInfoUcx {
  uint64_t addr; ///< Target memory address
  uint32_t rkey; ///< Target remote access key
};

struct ComputeNodeInfoUcx {
  BufferInfoUcx data;
  BufferInfoUcx desc;
  uint32_t index;
  uint32_t data_buffer_size_exp;
  uint32_t desc_buffer_size_exp;
};

#pragma pack()
