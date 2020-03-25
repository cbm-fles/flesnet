#include "GdpbEpochToMsSorter.hpp"

#include <iomanip>

void fles::GdpbEpochToMsSorter::process() {
  constexpr uint32_t kuBytesPerMessage = 8;

  // combine the contents of two consecutive microslices
  while (!this->input.empty()) {
    auto msInput = input.front();
    this->input.pop_front();

    // If not integer number of message in input buffer, print warning/error
    if (0 != (msInput->desc().size % kuBytesPerMessage)) {
      std::cerr << "fles::GdpbEpochToMsSorter::process => Warning, "
                << "the input microslice buffer does NOT contain only complete "
                   "nDPB messages!"
                << std::endl;
    }

    // Compute the number of complete messages in the input microslice buffer
    uint32_t uNbMessages =
        (msInput->desc().size - (msInput->desc().size % kuBytesPerMessage)) /
        kuBytesPerMessage;

    // Get access to microslice content in "buf" => Typically uint8_t* from
    // microslice "content"
    //      uint8_t* buff  = msInput->content();
    const uint64_t* pInBuff =
        reinterpret_cast<const uint64_t*>(msInput->content());
    for (uint32_t uIdx = 0; uIdx < uNbMessages; uIdx++) {
      uint64_t ulData = static_cast<uint64_t>(pInBuff[uIdx]);
      ngdpb::Message mess(ulData);

      std::cout << " IN " << std::hex << std::setw(8) << ulData << " "
                << std::dec << std::endl;

      if (true == mess.isHitMsg() || true == mess.isSyncMsg() ||
          true == mess.isAuxMsg()) {
        std::cout << "+++> Found a nDPB related message in "
                     "GdpbEpochToMsSorter, ignoring it! "
                  << "Please check your data input to make sure this is OK !!!!"
                  << std::endl;
        continue;
      } // if gDPB related message

      if (true == mess.isEpochMsg()) {
        // Expand epoch to 64 bits with a cycle counter in higher bits for
        // longer time sorting
        if ((mess.getEpochNumber() < fuCurrentEpoch) &&
            (0xEFFFFFFF < fuCurrentEpoch - mess.getEpochNumber())) {
          fuCurrentEpochCycle++;
        }

        fuCurrentEpoch = mess.getEpochNumber();

        fulCurrentLongEpoch =
            (static_cast<uint64_t>(fuCurrentEpochCycle) << 32) + fuCurrentEpoch;

        if (true == fbFirstEpFound) {
          fuNbEpInBuff++; // increase number of full epochs in buffer
                          /*
                                         // Check if we have enough epochs to fill a MS
                                         if( fuNbEpPerMs == fuNbEpInBuff )
                                         {
                                            // copy input descriptor
                                            MicrosliceDescriptor desc = msInput->desc();
                                            // Update Ms index/start time from epoch value
                             (later need the epoch duration)                 desc.idx  =
                             fulCurrentLongEpoch -                 fuNbEpPerMs; // *
                             kiEpochLengthNs
                                            // Update Ms size
                                            desc.size = fmsFullMsgBuffer.size();
                
                                            // Convert the multiset of messages to a vector of
                             bytes                 std::vector<uint8_t> content;                 for( auto
                             itMess =                 fmsFullMsgBuffer.begin(); itMess !=
                             fmsFullMsgBuffer.end();                 itMess++)
                                            {
                                               content.push_back( static_cast<uint8_t>(
                             ((*itMess).getData() & 0x00000000000000FFUL)      ) );
                                               content.push_back( static_cast<uint8_t>(
                             ((*itMess).getData() & 0x000000000000FF00UL) >>  8) );
                                               content.push_back( static_cast<uint8_t>(
                             ((*itMess).getData() & 0x0000000000FF0000UL) >> 16) );
                                               content.push_back( static_cast<uint8_t>(
                             ((*itMess).getData() & 0x00000000FF000000UL) >> 24) );
                                               content.push_back( static_cast<uint8_t>(
                             ((*itMess).getData() & 0x000000FF00000000UL) >> 32) );
                                               content.push_back( static_cast<uint8_t>(
                             ((*itMess).getData() & 0x0000FF0000000000UL) >> 40) );
                                               content.push_back( static_cast<uint8_t>(
                             ((*itMess).getData() & 0x00FF000000000000UL) >> 48) );
                                               content.push_back( static_cast<uint8_t>(
                             ((*itMess).getData() & 0xFF00000000000000UL) >> 56) );
                
                                               std::cout << std::hex << std::setw(8) <<
                             (*itMess).getData() << " "
                                                         << std::dec << std::endl;
                                            } // for( auto itMess = fmsFullMsgBuffer.begin();
                             itMess < fmsFullMsgBuffer.end(); itMess++)
                
                                            std::unique_ptr<StorableMicroslice> sortedMs(
                                                   new StorableMicroslice(desc, content));
                                            output.push(std::move(sortedMs));
                
                                            // Re-initialize the sorting buffer
                                            fmsFullMsgBuffer.clear();
                                            fuNbEpInBuff = 0;
                                         } // if( fuNbEpPerMs == fuNbEpInBuff )
                          */
        }                 // if( true == fbFirstEpFound )
        else {
          fbFirstEpFound = true;
        }
      } // if( true == mess.isEpochMsg() )
      else if (true == mess.isEpoch2Msg()) {
        uint16_t usChipIdx = mess.getGdpbGenChipId();
        if (kusMaxNbGet4 <= usChipIdx) {
          std::cout
              << "++++> Found an epoch2 message with ChipId out of bound in "
                 "GdpbEpochToMsSorter, ignoring it! "
              << "Please check your data input to make sure this is OK !!!!"
              << std::endl
              << "+++++> Read " << usChipIdx << " VS max. of " << kusMaxNbGet4
              << std::endl;
          continue;
        } // if( kuMaxNbGet4 <= usChipIdx )

        if (0 == ((fulMaskGet4 >> usChipIdx) & 0x1)) {
          std::cout
              << "++++> Found an epoch2 message from a masked Chip in "
                 "GdpbEpochToMsSorter, ignoring it! "
              << "Please check your data input to make sure this is OK !!!!"
              << std::endl;
          continue;
        } // if( 0 == ( (fulMaskGet4 >> usChipIdx) & 0x1 ) )

        // Expand epoch to 64 bits with a cycle counter in higher bits for
        // longer time sorting
        if ((mess.getEpochNumber() < fuCurrentEpoch2[usChipIdx]) &&
            (0xEFFFFFFF <
             fuCurrentEpoch2[usChipIdx] - mess.getGdpbEpEpochNb())) {
          fuCurrentEpochCycle2[usChipIdx]++;
        }

        fuCurrentEpoch2[usChipIdx] = mess.getGdpbEpEpochNb();

        fulCurrentLongEpoch2[usChipIdx] =
            (static_cast<uint64_t>(fuCurrentEpochCycle2[usChipIdx]) << 32) +
            fuCurrentEpoch2[usChipIdx];
      } // else if( true == mess.isEpoch2Msg() )

      // If the first epoch was found, start saving messages in the sorting
      // buffer
      if (true == fbFirstEpFound) {
        //            fmsMsgBuffer.insert( mess );
        if (true == mess.isEpochMsg() || true == mess.isSysMsg()) {
          ngdpb::FullMessage fullmess(mess, fulCurrentLongEpoch);
          fmsFullMsgBuffer.insert(fullmess);
        } // if DPB related message
        else if (true == mess.isEpoch2Msg() || true == mess.isGet4Msg() ||
                 true == mess.isGet4SlCtrMsg() ||
                 true == mess.isGet4Hit32Msg() || true == mess.isGet4SysMsg()) {
          uint16_t usChipIdx = mess.getGdpbGenChipId();

          if (kusMaxNbGet4 <= usChipIdx) {
            std::cout
                << "++++> Found a message with ChipId out of bound in "
                   "GdpbEpochToMsSorter, ignoring it! "
                << "Please check your data input to make sure this is OK !!!!"
                << std::endl
                << "+++++> Read " << usChipIdx << " VS max. of " << kusMaxNbGet4
                << std::endl;
            continue;
          } // if( kuMaxNbGet4 <= usChipIdx )

          if (0 == ((fulMaskGet4 >> usChipIdx) & 0x1)) {
            std::cout
                << "++++> Found a message from a masked Chip in "
                   "GdpbEpochToMsSorter, ignoring it! "
                << "Please check your data input to make sure this is OK !!!!"
                << std::endl;
            continue;
          } // if( 0 == ( (fulMaskGet4 >> usChipIdx) & 0x1 ) )

          ngdpb::FullMessage fullmess(mess, fulCurrentLongEpoch2[usChipIdx]);
          fmsFullMsgBuffer.insert(fullmess);
        } // if gDPB related message
      }   // if( true == fbFirstEpFound )
    }     // for (uint32_t uIdx = 0; uIdx < uNbMessages; uIdx ++)

  } // while (0 < this->input.size() )
}
