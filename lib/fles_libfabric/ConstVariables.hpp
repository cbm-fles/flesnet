// Copyright 2017 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#pragma once

#include <chrono>
#include <iostream>

#pragma pack(1)
namespace tl_libfabric {
struct ConstVariables {
  const static uint32_t MINUS_ONE = -1;
  const static uint32_t ZERO = 0;
  const static uint32_t ONE_HUNDRED = 100;

  const static uint32_t MAX_HISTORY_SIZE =
      500000; // high for logging ... should be small ~100-200

  const static uint32_t HEARTBEAT_TIMEOUT_HISTORY_SIZE =
      10000; // high for unstable networks

  const static uint32_t HEARTBEAT_INACTIVE_FACTOR =
      20; // Factor of timeout value before considering a connection is in
          // active

  const static uint32_t HEARTBEAT_TIMEOUT_FACTOR =
      25; // Factor of timeout value before considering a connection timed out

  const static uint16_t MAX_DESCRIPTOR_ARRAY_SIZE = 10;

  const static uint16_t MAX_COMPUTE_NODE_COUNT = 300;

  const static uint64_t HEARTBEAT_TIMEOUT = 1000000; // in microseconds

  const static uint32_t HEARTBEAT_INACTIVE_RETRY_COUNT = 3;

  const static uint64_t TIMESLICE_TIMEOUT = 200000000; // in microseconds

  const static uint64_t STATUS_MESSAGE_TAG = 10;
  const static uint64_t HEARTBEAT_MESSAGE_TAG = 20;
  ///-----
  const static uint32_t MAX_MEDIAN_VALUES = 10;
  ///-----/
};
} // namespace tl_libfabric
#pragma pack()
