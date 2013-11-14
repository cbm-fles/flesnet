// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#pragma pack(1)

/// Structure representing a set of compute node buffer positions.
struct ComputeNodeBufferPosition
{
    uint64_t data; ///< The position in the data buffer.
    uint64_t desc; ///< The position in the description buffer.
    bool operator<(const ComputeNodeBufferPosition& rhs) const
    {
        return desc < rhs.desc;
    }
    bool operator>(const ComputeNodeBufferPosition& rhs) const
    {
        return desc > rhs.desc;
    }
    bool operator==(const ComputeNodeBufferPosition& rhs) const
    {
        return desc == rhs.desc && data == rhs.data;
    }
    bool operator!=(const ComputeNodeBufferPosition& rhs) const
    {
        return desc != rhs.desc || data != rhs.data;
    }
};

constexpr ComputeNodeBufferPosition CN_WP_FINAL = {UINT64_MAX, UINT64_MAX};

#pragma pack()
