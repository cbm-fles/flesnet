// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#pragma once

#include <cstdint>

#pragma pack(1)

namespace tl_libfabric {
/// Access information for a remote memory region.
struct BufferInfo {
  uint64_t addr; ///< Target memory address
  uint64_t rkey; ///< Target remote access key
};

struct ComputeNodeInfo {
  BufferInfo data;
  BufferInfo desc;
  uint32_t index;
  uint32_t data_buffer_size_exp;
  uint32_t desc_buffer_size_exp;
};
} // namespace tl_libfabric
#pragma pack()
