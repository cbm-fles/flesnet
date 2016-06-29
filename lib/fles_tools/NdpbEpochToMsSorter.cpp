#include "NdpbEpochToMsSorter.hpp"

void fles::NdpbEpochToMsSorter::process()
{
   constexpr uint32_t kuBytesPerMessage = 8;

   // combine the contents of two consecutive microslices
   while (0 < this->input.size() ) {
      auto msInput = input.front();
      this->input.pop_front();

      // If not integer number of message in input buffer, print warning/error
      if( 0 != (msInput->desc().size % kuBytesPerMessage) )
         std::cerr << "fles::NdpbEpochToMsSorter::process => Warning, "
                   << "the input microslice buffer does NOT contain only complete nDPB messages!"
                   << std::endl;

      // Compute the number of complete messages in the input microslice buffer
      uint32_t uNbMessages = (msInput->desc().size - (msInput->desc().size%kuBytesPerMessage) )
                            / kuBytesPerMessage;

      // Get access to microslice content in "buf" => Typically uint8_t* from microslice "content"
//      uint8_t* buff  = msInput->content();
      const uint64_t* pInBuff = reinterpret_cast<const uint64_t*>( msInput->content() );
      for (uint32_t uIdx = 0; uIdx < uNbMessages; uIdx ++)
      {
         uint64_t ulData = static_cast<uint64_t>( pInBuff[uIdx] );
         ngdpb::Message mess( ulData );

         if( true == mess.isEpochMsg() )
         {
            fulCurrentEpoch = mess.getEpochNumber(); // Later expand to 64 bits with a cycle counter

            if( true == fbFirstEpFound )
            {
               fuNbEpInBuff++; // increase number of full epochs in buffer

               // Check if we have enough epochs to fill a MS
               if( fuNbEpPerMs == fuNbEpInBuff )
               {
                  // copy input descriptor
                  MicrosliceDescriptor desc = msInput->desc();
                  // Update Ms index/start time from epoch value (later need the epoch duration)
                  desc.idx  = fulCurrentEpoch - fuNbEpPerMs; // * kiEpochLengthNs
                  // Update Ms size
                  desc.size = fmsMsgBuffer.size();

                  // Convert the multiset of messages to a vector of bytes
                  std::vector<uint8_t> content;
                  for( auto itMess = fmsMsgBuffer.begin(); itMess != fmsMsgBuffer.end(); itMess++)
                  {
                     content.push_back( static_cast<uint8_t>( ((*itMess).getData() & 0xFF00000000000000UL) > 56) );
                     content.push_back( static_cast<uint8_t>( ((*itMess).getData() & 0x00FF000000000000UL) > 48) );
                     content.push_back( static_cast<uint8_t>( ((*itMess).getData() & 0x0000FF0000000000UL) > 40) );
                     content.push_back( static_cast<uint8_t>( ((*itMess).getData() & 0x000000FF00000000UL) > 32) );
                     content.push_back( static_cast<uint8_t>( ((*itMess).getData() & 0x00000000FF000000UL) > 24) );
                     content.push_back( static_cast<uint8_t>( ((*itMess).getData() & 0x0000000000FF0000UL) > 16) );
                     content.push_back( static_cast<uint8_t>( ((*itMess).getData() & 0x000000000000FF00UL) >  8) );
                     content.push_back( static_cast<uint8_t>( ((*itMess).getData() & 0x00000000000000FFUL)     ) );
                  } // for( auto itMess = fmsMsgBuffer.begin(); itMess < fmsMsgBuffer.end(); itMess++)

                  std::unique_ptr<StorableMicroslice> sortedMs(
                         new StorableMicroslice(desc, content));
                  output.push(std::move(sortedMs));

                  // Re-initialize the sorting buffer
                  fmsMsgBuffer.clear();
                  fuNbEpInBuff = 0;
               } // if( fuNbEpPerMs == fuNbEpInBuff )
            } // if( true == fbFirstEpFound )
               else fbFirstEpFound = true;
         } // if( true == mess.isEpochMsg() )

         // If the first epoch was found, start saving messages in the sorting buffer
         if( true == fbFirstEpFound )
         {
            fmsMsgBuffer.insert( mess );
         } // if( true == fbFirstEpFound )
      } // for (uint32_t uIdx = 0; uIdx < uNbMessages; uIdx ++)
/*
          uint32_t fuNbEpPerMs;
          std::multiset<ngdpb::Message> fmsMsgBuffer
          uint32_t fuNbEpInBuff;
          bool     fbFirstEpFound;
          uint64_t fulCurrentEpoch;

      content.assign(item1->content(),
                     item1->content() + item1->desc().size);
*/
   } // while (0 < this->input.size() )
}
