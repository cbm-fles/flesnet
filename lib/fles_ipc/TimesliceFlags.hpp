// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the flags of timeslices and their components.
#pragma once

#include <cstdint>

namespace fles::tsb {

enum class TsComponentFlag : uint32_t {
  None = 0,

  // Microslices are missing in this component
  MissingMicroslices = 1 << 0,

  // One or more microslices in this component have their "OverflowFlim" flag
  // set
  OverflowFlim = 1 << 1
};

enum class TsFlag : uint32_t {
  None = 0,

  // One or more components are missing microslices
  // (i.e., at least one component has
  // StComponentFlag::IsMissingMicroslices set)
  MissingMicroslices = 1 << 0,

  // One or more components in this subtimeslice have
  // StComponentFlag::OverflowFlim set, i.e., at least one microslice has its
  // "OverflowFlim" flag set
  OverflowFlim = 1 << 1,

  // Subtimeslice is incomplete due to missing components
  MissingComponents = 1 << 2,

  // Timeslice is incomplete due to missing subtimeslices
  MissingSubtimeslices = 1 << 3,
};

} // namespace fles::tsb
