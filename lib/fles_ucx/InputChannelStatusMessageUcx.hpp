// Copyright 2014-2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ComputeNodeBufferPositionUcx.hpp"

#pragma pack(1)

/// Structure representing a status update message sent from input channel to
/// compute buffer.
struct InputChannelStatusMessageUcx {
  ComputeNodeBufferPositionUcx wp;
  bool abort;
  bool final;
};

#pragma pack()
