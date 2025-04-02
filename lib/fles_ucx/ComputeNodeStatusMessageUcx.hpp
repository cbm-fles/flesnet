// Copyright 2014-2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ComputeNodeBufferPositionUcx.hpp"

#pragma pack(1)

/// Structure representing a status update message sent from compute buffer to
/// input channel.
struct ComputeNodeStatusMessageUcx {
  ComputeNodeBufferPositionUcx ack;
  bool request_abort;
  bool final;
};

#pragma pack()
