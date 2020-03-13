// Copyright 2016 Pierre-Alain Loizeau <p.-a.loizeau@gsi.de>
/// \file
/// \brief Microslice stream filter based on fles::Filter for the nDPB data:
///        use the epoch message as output MS delimiter and time sort messages.
#pragma once

#include "Filter.hpp"
#include "Microslice.hpp"
#include "StorableMicroslice.hpp"

#include "rocMess_wGet4v1.h"

#include <set>
#include <vector>

namespace fles {

class NdpbEpochToMsSorter
    : public BufferingFilter<Microslice, StorableMicroslice> {
public:
  NdpbEpochToMsSorter(uint32_t uEpPerMs = 1, bool bSortMsgs = false)
      : fuNbEpPerMs(uEpPerMs), fbMsgSorting(bSortMsgs){};

private:
  void process() override;

  uint32_t fuNbEpPerMs;
  bool fbMsgSorting;
  std::multiset<ngdpb::FullMessage> fmsFullMsgBuffer;
  std::vector<ngdpb::Message> fvMsgBuffer;
  uint32_t fuNbEpInBuff{0};
  bool fbFirstEpFound{false};
  uint32_t fuCurrentEpoch{0};
  uint32_t fuCurrentEpochCycle{0};
  uint64_t fulCurrentLongEpoch{0};
};
} // namespace fles
