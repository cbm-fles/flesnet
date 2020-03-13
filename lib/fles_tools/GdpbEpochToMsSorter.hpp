// Copyright 2016 Pierre-Alain Loizeau <p.-a.loizeau@gsi.de>
/// \file
/// \brief Microslice stream filter based on fles::Filter for the gDPB data:
///        use the epoch2 messages as output MS delimiter and time sort
///        messages.
#pragma once

#include "Filter.hpp"
#include "Microslice.hpp"
#include "StorableMicroslice.hpp"

#include "rocMess_wGet4v1.h"

#include <set>

#pragma clang diagnostic ignored "-Wunused-private-field"

namespace fles {

class GdpbEpochToMsSorter
    : public BufferingFilter<Microslice, StorableMicroslice> {
public:
  GdpbEpochToMsSorter(uint32_t uEpPerMs = 1, uint64_t ulMaskGet4 = 0x0)
      : fuNbEpPerMs(uEpPerMs), fulMaskGet4(ulMaskGet4){};

private:
  void process() override;

  static const uint16_t kusMaxNbGet4 = 48; // 6 * 8 GET4s max per gDPB

  uint32_t fuNbEpPerMs;
  std::multiset<ngdpb::FullMessage> fmsFullMsgBuffer;
  uint32_t fuNbEpInBuff{0};

  bool fbFirstEpFound{false};
  uint32_t fuCurrentEpoch{0};
  uint32_t fuCurrentEpochCycle{0};
  uint64_t fulCurrentLongEpoch{0};

  bool fbFirstEp2Found[kusMaxNbGet4]{};
  uint32_t fuCurrentEpoch2[kusMaxNbGet4]{};
  uint32_t fuCurrentEpochCycle2[kusMaxNbGet4]{};
  uint64_t fulCurrentLongEpoch2[kusMaxNbGet4]{};
  bool fbAllFirstEp2Found{false};
  bool fbAllCurrentEp2Found{false};

  uint64_t fulMaskGet4;
};
} // namespace fles
