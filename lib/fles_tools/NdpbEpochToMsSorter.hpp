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

namespace fles
{

   class NdpbEpochToMsSorter
       : public BufferingFilter<Microslice, StorableMicroslice>
   {
          NdpbEpochToMsSorter( uint32_t uEpPerMs = 1 ) :
             BufferingFilter(),
             fuNbEpPerMs(uEpPerMs), fmsMsgBuffer(), fuNbEpInBuff(0),
             fbFirstEpFound(false), fulCurrentEpoch(0)
             {};

      private:
          void process() override;

          uint32_t fuNbEpPerMs;
          std::multiset<ngdpb::Message> fmsMsgBuffer;
          uint32_t fuNbEpInBuff;
          bool     fbFirstEpFound;
          uint64_t fulCurrentEpoch;
   };
} // namespace fles
