// Copyright 2014 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ComputeNodeBufferPosition.hpp"

#pragma pack(1)

/// Structure representing a status update message sent from input channel to
/// compute buffer.
struct InputChannelStatusMessage {
    ComputeNodeBufferPosition wp;
    bool abort;
    bool final;
};

#pragma pack()
