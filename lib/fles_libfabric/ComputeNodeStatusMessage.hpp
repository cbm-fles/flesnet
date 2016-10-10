// Copyright 2014 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ComputeNodeBufferPosition.hpp"
#include "ComputeNodeInfo.hpp"

#pragma pack(1)

/// Structure representing a status update message sent from compute buffer to
/// input channel.
struct ComputeNodeStatusMessage {
    ComputeNodeBufferPosition ack;
    bool request_abort;
    bool final;
    //
    bool connect;
    ComputeNodeInfo info;
    // address must be not null if connect = true
    unsigned char my_address[64];
};

#pragma pack()
