#include "NdpbEpochToMsSorter.hpp"

#include <iomanip>

void fles::NdpbEpochToMsSorter::process() {
  constexpr uint32_t kuBytesPerMessage = 8;

  // combine the contents of two consecutive microslices
  while (!this->input.empty()) {
    auto msInput = input.front();
    this->input.pop_front();

    // If not integer number of message in input buffer, print warning/error
    if (0 != (msInput->desc().size % kuBytesPerMessage)) {
      std::cerr << "fles::NdpbEpochToMsSorter::process => Warning, "
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
    const uint64_t* pInBuff =
        reinterpret_cast<const uint64_t*>(msInput->content());
    for (uint32_t uIdx = 0; uIdx < uNbMessages; uIdx++) {
      uint64_t ulData = static_cast<uint64_t>(pInBuff[uIdx]);
      ngdpb::Message mess(ulData);

      //         std::cout << " IN " << std::hex << std::setw(8) << ulData << "
      //         "
      //                   << std::dec << std::endl;

      if (true == mess.isEpoch2Msg() || true == mess.isGet4Msg() ||
          true == mess.isGet4SlCtrMsg() || true == mess.isGet4Hit32Msg() ||
          true == mess.isGet4SysMsg()) {
        std::cout << "+++> Found a GET4 related message in "
                     "NdpbEpochToMsSorter, ignoring it! "
                  << "Please check your data input to make sure this is OK !!!!"
                  << std::endl;
        continue;
      } // if gDPB related message

      // Ignore the SYNC messages as they are used only to get all EPOCH
      // messages in nDPB
      if (true == mess.isSyncMsg()) {
        continue;
      }

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

          // Check if we have enough epochs to fill a MS
          if (fuNbEpPerMs == fuNbEpInBuff) {
            // copy input descriptor
            MicrosliceDescriptor desc = msInput->desc();
            // Update Ms index/start time from epoch value (later need the epoch
            // duration)
            desc.idx = fulCurrentLongEpoch - fuNbEpPerMs; // * kiEpochLengthNs
            // Update Ms size
            desc.size = fmsFullMsgBuffer.size();
            // Update the Format and version version
            // 0xE. for Experimental
            // 0x.0 for Raw data
            // 0x.1 for (Epoch-)aligned Microslices
            // 0x.2 for Time-sorted data
            if (true == fbMsgSorting) {
              desc.sys_ver = 0xE2;
            } else {
              desc.sys_ver = 0xE1;
            }

            // Convert the multiset of messages to a vector of bytes
            std::vector<uint8_t> content;
            if (true == fbMsgSorting) {
              for (const auto& itMess : fmsFullMsgBuffer) {
                /* Wrong Endianness?!?
                                     content.push_back( static_cast<uint8_t>(
                   ((*itMess).getData() & 0xFF00000000000000UL) >> 56) );
                                     content.push_back( static_cast<uint8_t>(
                   ((*itMess).getData() & 0x00FF000000000000UL) >> 48) );
                                     content.push_back( static_cast<uint8_t>(
                   ((*itMess).getData() & 0x0000FF0000000000UL) >> 40) );
                                     content.push_back( static_cast<uint8_t>(
                   ((*itMess).getData() & 0x000000FF00000000UL) >> 32) );
                                     content.push_back( static_cast<uint8_t>(
                   ((*itMess).getData() & 0x00000000FF000000UL) >> 24) );
                                     content.push_back( static_cast<uint8_t>(
                   ((*itMess).getData() & 0x0000000000FF0000UL) >> 16) );
                                     content.push_back( static_cast<uint8_t>(
                   ((*itMess).getData() & 0x000000000000FF00UL) >>  8) );
                                     content.push_back( static_cast<uint8_t>(
                   ((*itMess).getData() & 0x00000000000000FFUL)      ) );
                */
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0x00000000000000FFUL)));
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0x000000000000FF00UL) >> 8));
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0x0000000000FF0000UL) >> 16));
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0x00000000FF000000UL) >> 24));
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0x000000FF00000000UL) >> 32));
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0x0000FF0000000000UL) >> 40));
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0x00FF000000000000UL) >> 48));
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0xFF00000000000000UL) >> 56));

                /*
                                     std::cout << std::hex <<
                   (*itMess).getData() << " "
                                               << std::hex << static_cast<
                   uint16_t>( content[ content.size() -8 ] ) << " "
                                               << std::hex << static_cast<
                   uint16_t>( content[ content.size() -7 ] ) << " "
                                               << std::hex << static_cast<
                   uint16_t>( content[ content.size() -6 ] ) << " "
                                               << std::hex << static_cast<
                   uint16_t>( content[ content.size() -5 ] ) << " "
                                               << std::hex << static_cast<
                   uint16_t>( content[ content.size() -4 ] ) << " "
                                               << std::hex << static_cast<
                   uint16_t>( content[ content.size() -3 ] ) << " "
                                               << std::hex << static_cast<
                   uint16_t>( content[ content.size() -2 ] ) << " "
                                               << std::hex << static_cast<
                   uint16_t>( content[ content.size() -1 ] ) << " "
                                               << std::dec << std::endl;
                */
                //                     std::cout << std::hex << std::setw(8) <<
                //                     (*itMess).getData() << " "
                //                               << std::dec << std::endl;
              } // for( auto itMess = fmsFullMsgBuffer.begin(); itMess <
                // fmsFullMsgBuffer.end(); itMess++)
            } else {
              for (auto& itMess : fvMsgBuffer) {
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0x00000000000000FFUL)));
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0x000000000000FF00UL) >> 8));
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0x0000000000FF0000UL) >> 16));
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0x00000000FF000000UL) >> 24));
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0x000000FF00000000UL) >> 32));
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0x0000FF0000000000UL) >> 40));
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0x00FF000000000000UL) >> 48));
                content.push_back(static_cast<uint8_t>(
                    (itMess.getData() & 0xFF00000000000000UL) >> 56));
              } // else for( auto itMess = fvMsgBuffer.begin(); itMess !=
            }
            // fvMsgBuffer.end(); itMess++)
            std::unique_ptr<StorableMicroslice> sortedMs(
                new StorableMicroslice(desc, content));
            output.push(std::move(sortedMs));

            // Re-initialize the sorting buffer
            if (true == fbMsgSorting) {
              fmsFullMsgBuffer.clear();
            } else {
              fvMsgBuffer.clear();
            }
            fuNbEpInBuff = 0;
          } // if( fuNbEpPerMs == fuNbEpInBuff )
        }   // if( true == fbFirstEpFound )
        else {
          fbFirstEpFound = true;
        }
      } // if( true == mess.isEpochMsg() )

      // If the first epoch was found, start saving messages in the sorting
      // buffer
      if (true == fbFirstEpFound) {
        // If messages sorting enabled, do it with multiset and FullMessage "<"
        // OP
        if (true == fbMsgSorting) {
          ngdpb::FullMessage fullmess(mess, fulCurrentLongEpoch);
          fmsFullMsgBuffer.insert(fullmess);
        } // if( true == fbMsgSorting )
          // Otherwise the simpler "sorting by epoch" is done just by epoch
          // definition!
          // => vector is enough
        else {
          fvMsgBuffer.push_back(mess);
        }
      } // if( true == fbFirstEpFound )
    }   // for (uint32_t uIdx = 0; uIdx < uNbMessages; uIdx ++)
  }     // while (0 < this->input.size() )
}
