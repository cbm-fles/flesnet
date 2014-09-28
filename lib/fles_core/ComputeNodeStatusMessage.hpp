// Copyright 2014 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ComputeNodeBufferPosition.hpp"

#pragma pack(1)

/// Structure representing a status update message sent from compute buffer to
/// input channel.
struct ComputeNodeStatusMessage
{
    ComputeNodeBufferPosition ack;
    bool final;
};

#pragma pack()
