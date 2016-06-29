#include "rocMess_wGet4v1.h"

// Specific headers

// std C++ lib headers
#include <stdio.h>
#include <string.h>

//#include <iostream>

//----------------------------------------------------------------------------
//! Returns expanded and adjusted time of message (in ns)
//! epoch should correspond to the message type - epoch2 for Get4, epoch for all others
//! When converting from GET4 (156Mhz) to SYNC (250 Mhz) clock, one should take into account
//! that initial value of 250 MHz timestamp counter is 0x80 or 512 ns. Therefore such offset
//! should be applied during conversion

uint64_t ngdpb::Message::getMsgFullTime(uint32_t epoch) const
{
   switch (getMessageType()) {
      case MSG_HIT:
         return FullTimeStamp(getNxLastEpoch() ? epoch - 1 : epoch, getNxTs());
      case MSG_EPOCH:
         return FullTimeStamp(getEpochNumber(), 0);
      case MSG_SYNC:
         return FullTimeStamp((getSyncEpochLSB() == (epoch & 0x1)) ? epoch : epoch - 1, getSyncTs());
      case MSG_AUX:
         return FullTimeStamp((getAuxEpochLSB() == (epoch & 0x1)) ? epoch : epoch - 1, getAuxTs());
      case MSG_EPOCH2:
         return FullTimeStamp2(getEpoch2Number(), 0) / 20 + 512;
      case MSG_GET4:
         return FullTimeStamp2(epoch, getGet4Ts()) / 20 + 512;
      case MSG_SYS:
         return FullTimeStamp(epoch, 0);

   }
   return 0;
}

//----------------------------------------------------------------------------
//! strict weak ordering operator, assumes same epoch for both messages
bool ngdpb::Message::operator<(const Message& other) const
{
   uint64_t uThisTs  = 0;
   uint64_t uOtherTs = 0;

   uint32_t uThisType  = this->getMessageType();
   uint32_t uOtherType = other.getMessageType();

   // if both GET4 hit messages, use the full timestamp info
   if( (MSG_GET4_32B == uThisType  || MSG_GET4 == uThisType )&&
       (MSG_GET4_32B == uOtherType || MSG_GET4 == uOtherType ) )
   {
      uThisTs  = this->getGdpbHitFullTs();
      uOtherTs = other.getGdpbHitFullTs();
      return uThisTs < uOtherTs;
   } // both GET4 hit (32b or 24b)

   // First find the timestamp of the current message
   if( MSG_GET4_32B == uThisType  || MSG_GET4 == uThisType )
   {
      uThisTs = ( this->getGdpbHitFullTs() ) / 20 + 512;
   } // if Hit GET4 message (24 or 32b)
   else if( MSG_GET4_SLC == uThisType || MSG_GET4_SYS == uThisType )
   {
      uThisTs = 0;
   } // if SLC or SYS GET4 message
      else uThisTs = this->getMsgFullTime( 0 );

   // Then find the timestamp of the current message
   if( MSG_GET4_32B == uOtherType  || MSG_GET4 == uOtherType )
   {
      uOtherTs = ( this->getGdpbHitFullTs() ) / 20 + 512;
   } // if Hit GET4 message (24 or 32b)
   else if( MSG_GET4_SLC == uOtherType || MSG_GET4_SYS == uOtherType )
   {
      uOtherTs = 0;
   } // if SLC or SYS GET4 message
      else uOtherTs = other.getMsgFullTime( 0 );

   return uThisTs < uOtherTs;
}
//----------------------------------------------------------------------------
//! Returns expanded and adjusted time of message in double (in ns)
//! epoch should correspond to the message type - epoch2 for Get4, epoch for all others

double ngdpb::Message::getMsgFullTimeD(uint32_t epoch) const
{
   switch (getMessageType()) {
      case MSG_EPOCH2:
         return FullTimeStamp2(getEpoch2Number(), 0) / 20. + 512.;
      case MSG_GET4:
      case MSG_GET4_32B:
         return FullTimeStamp2(epoch, getGdpbHitFullTs()) / 20. + 512.;
   }
   return getMsgFullTime(epoch);
}


//----------------------------------------------------------------------------
//! Returns the time difference between two expanded time stamps

uint64_t ngdpb::Message::CalcDistance(uint64_t start, uint64_t stop)
{
   if (start>stop) {
      stop += 0x3FFFFFFFFFFFLLU;
      if (start>stop) {
         printf("Epochs overflow error in CalcDistance\n");
         return 0;
      }
   }

   return stop - start;
}


//----------------------------------------------------------------------------
//! Returns the time difference between two expanded time stamps

double ngdpb::Message::CalcDistanceD(double start, double stop)
{
   if (start>stop) {
      stop += 0x3FFFFFFFFFFFLLU;
      if (start>stop) {
         printf("Epochs overflow error in CalcDistanceD\n");
         return 0.;
      }
   }

   return stop - start;
}



//----------------------------------------------------------------------------
//! Print message in human readable format to \a cout.
/*!
 * Prints a one line representation of the message in to \a cout.
 * See printData(std::ostream&, unsigned, uint32_t) const for full
 * documentation.
 */

void ngdpb::Message::printDataCout(unsigned kind, uint32_t epoch) const
{
  printData(msg_print_Cout, kind, epoch);
}

//----------------------------------------------------------------------------
//! Print message in human readable format to the Fairroot logger.
/*!
 * Prints a one line representation of the message in to the Fairroot logger.
 * TODO: Add coloring of possible
 * See printData(std::ostream&, unsigned, uint32_t) const for full
 * documentation.
 */

void ngdpb::Message::printDataLog(unsigned kind, uint32_t epoch) const
{
  printData(msg_print_FairLog, kind, epoch);
}

//----------------------------------------------------------------------------
//! Print message in binary or human readable format to a stream.
/*!
 * Prints a one line representation of the message in to a stream, selected by \a outType.
 * The stream is \a cout if \a outType is kFALSE and \a FairLogger if \a outType is kTRUE.
 * The parameter \a kind is mask with 4 bits
 * \li ngdpb::msg_print_Prefix (1) - ROC number and message type
 * \li ngdpb::msg_print_Data   (2) - print all message specific data fields
 * \li ngdpb::msg_print_Hex    (4) - print data as hex dump
 * \li ngdpb::msg_print_Human  (8) - print in human readable format
 *
 * If bit msg_print_Human in \a kind is not set, raw format
 * output is generated. All data fields are shown in hexadecimal.
 * This is the format of choice when chasing hardware problems at the bit level.
 *
 * If bit msg_print_Human is set, a more human readable output is generated.
 * The timestamp is shown as fully extended and adjusted time as
 * returned by the getMsgFullTime(uint32_t) const method.
 * All data fields are represented in decimal.
 *
 * \param os output stream
 * \param kind mask determing output format
 * \param epoch current epoch number (from last epoch message)
 *
 * Typical message output in human format looks like
\verbatim
Msg:7 Roc:1 SysType: 1 Nx:0 Data:        0 : DAQ started
Msg:7 Roc:1 SysType: 6 Nx:0 Data:        0 : FIFO reset
Msg:2 Roc:1 EPO @    0.536870912 Epo:     32768 0x00008000 Miss:   0
Msg:0 Roc:0 NOP (raw 80:40:82:0F:00:00)
Msg:2 Roc:1 EPO @    0.646627328 Epo:     39467 0x00009a2b Miss:   0
Msg:1 Roc:1 HIT @    0.646614333 Nx:2 Chn: 12 Ts: 3389-e( 8) Adc:2726 Pu:0 Of:0
Msg:1 Roc:1 HIT @    0.646630717 Nx:2 Chn: 13 Ts: 3389  ( 0) Adc:2745 Pu:0 Of:0
Msg:2 Roc:1 EPO @    0.805306368 Epo:     49152 0x0000c000 Miss:   0
Msg:3 Roc:1 SYN @    0.805306408 Chn:2 Ts:   40   Data:   49152 0x00c000 Flag:0
Msg:7 Roc:1 SysType: 2 Nx:0 Data:        0 : DAQ finished
\endverbatim
 *
 * Typical message output in binary format looks like
\verbatim
Msg:7 Roc:1 SysType: 1 Nx:0 Data:        0 : DAQ started
Msg:7 Roc:1 SysType: 6 Nx:0 Data:        0 : FIFO reset
Msg:2 Roc:1 Epoch:00008000 Missed:00
Msg:1 Roc:1 Nx:2 Chn:0d Ts:3ec9 Last:1 Msb:7 Adc:a22 Pup:0 Oflw:0
Msg:1 Roc:1 Nx:2 Chn:0e Ts:3ec9 Last:0 Msb:7 Adc:a18 Pup:0 Oflw:0
Msg:0 Roc:0 NOP (raw 80:40:82:0F:00:00)
Msg:2 Roc:1 Epoch:00010000 Missed:00
Msg:3 Roc:1 SyncChn:2 EpochLSB:0 Ts:0028 Data:010000 Flag:0
Msg:7 Roc:1 SysType: 2 Nx:0 Data:        0 : DAQ finished
\endverbatim
 */

//void ngdpb::Message::printData(std::ostream& os, unsigned kind, uint32_t epoch) const
void ngdpb::Message::printData(unsigned outType, unsigned kind, uint32_t epoch, std::ostream& os) const
{
   char buf[256];
   if (kind & msg_print_Hex) {
//      uint8_t* arr = static_cast<uint8_t*> ( &data );
      uint8_t* arr = (uint8_t*) ( &data );
      snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X ",
               arr[0], arr[1], arr[2], arr[3], arr[4], arr[5]);
//      os << buf;
      if( msg_print_Cout == outType)
         std::cout << buf;
      else if( msg_print_File == outType )
         os << buf;
   }

   if (kind & msg_print_Human) {
      double timeInSec = getMsgFullTimeD(epoch)/1.e9;
      int fifoFill = 0;

      switch (getMessageType()) {
         case MSG_HIT:
            fifoFill = getNxLtsMsb() - ((getNxTs()>>11)&0x7);
            if (getNxLastEpoch()) fifoFill += 8;
            snprintf(buf, sizeof(buf),
                  "Msg:%u Roc:%u ", getMessageType(), getRocNumber());
//            os << buf;
            if( msg_print_Cout == outType)
               std::cout << buf;
            else if( msg_print_File == outType )
               os << buf;

            snprintf(buf, sizeof(buf),
                  "HIT  @%15.9f Nx:%d Chn:%3d Ts:%5d%s(%2d) Adc:%4d Pu:%d Of:%d",
                  timeInSec, getNxNumber(), getNxChNum(), getNxTs(),
                  (getNxLastEpoch() ? "-e" : "  "),
                  fifoFill, getNxAdcValue(), getNxPileup(), getNxOverflow());
//            os << buf << std::endl;
            if( msg_print_Cout == outType)
               std::cout << buf << std::endl;
            else if( msg_print_File == outType )
               os << buf << std::endl;
            break;
         case MSG_EPOCH:
            snprintf(buf, sizeof(buf),
                  "Msg:%u Roc:%u ", getMessageType(), getRocNumber());
//            os << buf;
            if( msg_print_Cout == outType)
               std::cout << buf;
            else if( msg_print_File == outType )
               os << buf;

            snprintf(buf, sizeof(buf),
                  "EPO  @%15.9f Epo:%10u 0x%08x Miss: %3d%c",
                  timeInSec, getEpochNumber(), getEpochNumber(),
                  getEpochMissed(), (getEpochMissed()==0xff) ? '+' : ' ');
//            os << buf << std::endl;
            if( msg_print_Cout == outType)
               std::cout << buf << std::endl;
            else if( msg_print_File == outType )
               os << buf << std::endl;
            break;
         case MSG_SYNC:
            snprintf(buf, sizeof(buf),
                  "Msg:%u Roc:%u ", getMessageType(), getRocNumber());
//            os << buf;
            if( msg_print_Cout == outType)
               std::cout << buf;
            else if( msg_print_File == outType )
               os << buf;

            snprintf(buf, sizeof(buf),
                  "SYN  @%15.9f Chn:%d Ts:%5d%s Data:%8d 0x%06x Flag:%d",
                  timeInSec, getSyncChNum(), getSyncTs(),
                  ((getSyncEpochLSB() != (epoch&0x1)) ? "-e" : "  "),
                  getSyncData(), getSyncData(), getSyncStFlag());
//            os << buf << std::endl;
            if( msg_print_Cout == outType)
               std::cout << buf << std::endl;
            else if( msg_print_File == outType )
               os << buf << std::endl;
            break;
         case MSG_AUX:
            snprintf(buf, sizeof(buf),
                  "Msg:%u Roc:%u ", getMessageType(), getRocNumber());
//            os << buf;
            if( msg_print_Cout == outType)
               std::cout << buf;
            else if( msg_print_File == outType )
               os << buf;

            snprintf(buf, sizeof(buf),
                  "AUX  @%15.9f Chn:%d Ts:%5d%s Falling:%d Overflow:%d",
                  timeInSec, getAuxChNum(), getAuxTs(),
                  ((getAuxEpochLSB() != (epoch&0x1)) ? "-e" : "  "),
                  getAuxFalling(), getAuxOverflow());
//            os << buf << std::endl;
            if( msg_print_Cout == outType)
               std::cout << buf << std::endl;
            else if( msg_print_File == outType )
               os << buf << std::endl;
            break;
         case MSG_EPOCH2:
            snprintf(buf, sizeof(buf),
                  "Msg:%u Roc:%u ", getMessageType(), getRocNumber());
//            os << buf;
            if( msg_print_Cout == outType)
               std::cout << buf;
            else if( msg_print_File == outType )
               os << buf;

            snprintf(buf, sizeof(buf),
                  "EPO2 @%17.11f Get4:%2d Epoche2:%10u 0x%08x StampTime:%2d Sync:%x Dataloss:%x Epochloss:%x Epochmissmatch:%x", 
                        timeInSec, getEpoch2ChipNumber(), getEpoch2Number(), getEpoch2Number(), 
                        getEpoch2StampTime(), getEpoch2Sync(), getEpoch2DataLost(), getEpoch2EpochLost(), getEpoch2EpochMissmatch());
//            os << buf << std::endl;
            if( msg_print_Cout == outType)
               std::cout << buf << std::endl;
            else if( msg_print_File == outType )
               os << buf << std::endl;
            break;
         case MSG_GET4:
            snprintf(buf, sizeof(buf),
                  "Msg:%u Roc:%u ", getMessageType(), getRocNumber());
//            os << buf;
            if( msg_print_Cout == outType)
               std::cout << buf;
            else if( msg_print_File == outType )
               os << buf;

            snprintf(buf, sizeof(buf),
                  "Get4 @%17.11f Get4:%2d Chn:%3d Edge:%1d Ts:%7d CRC8:%3d",
                  timeInSec, getGet4Number(), getGet4ChNum(), getGet4Edge(), getGet4Ts(), getGet4CRC() );
//            os << buf << std::endl;
            if( msg_print_Cout == outType)
               std::cout << buf << std::endl;
            else if( msg_print_File == outType )
               os << buf << std::endl;
            break;
         default:
            kind = kind & ~msg_print_Human;
            if (kind==0) kind = msg_print_Prefix | msg_print_Data;
      }

      // return, if message was correctly printed in human-readable form
      if (kind & msg_print_Human) return;
   }

   if (kind & msg_print_Prefix) {
      snprintf(buf, sizeof(buf), "Msg:%u Roc:%u ", getMessageType(), getRocNumber());
//      os << buf;
      if( msg_print_Cout == outType)
         std::cout << buf;
      else if( msg_print_File == outType )
         os << buf;
   }

   if (kind & msg_print_Data) {
//      uint8_t* arr = static_cast<uint8_t*> ( &data );
      uint8_t* arr = (uint8_t*) ( &data );
      switch (getMessageType()) {
        case MSG_NOP:
           snprintf(buf, sizeof(buf), "NOP (raw %02X:%02X:%02X:%02X:%02X:%02X)",
                    arr[0], arr[1], arr[2], arr[3], arr[4], arr[5]);
            break;
         case MSG_HIT:
            snprintf(buf, sizeof(buf), "Nx:%1x Chn:%02x Ts:%04x Last:%1x Msb:%1x Adc:%03x Pup:%1x Oflw:%1x",
                    getNxNumber(), getNxChNum(), getNxTs(), getNxLastEpoch(),
                    getNxLtsMsb(), getNxAdcValue(), getNxPileup(),
                    getNxOverflow());
            break;
         case MSG_EPOCH:
            snprintf(buf, sizeof(buf), "Epoch:%08x Missed:%02x",
                    getEpochNumber(), getEpochMissed());
            break;
         case MSG_SYNC:
            snprintf(buf, sizeof(buf), "SyncChn:%1x EpochLSB:%1x Ts:%04x Data:%06x Flag:%1x",
                    getSyncChNum(), getSyncEpochLSB(), getSyncTs(),
                    getSyncData(), getSyncStFlag());
            break;
         case MSG_AUX:
            snprintf(buf, sizeof(buf), "AuxChn:%02x EpochLSB:%1x Ts:%04x Falling:%1x Overflow:%1x",
                    getAuxChNum(), getAuxEpochLSB(), getAuxTs(),
                    getAuxFalling(), getAuxOverflow());
            break;
         case MSG_EPOCH2:
            snprintf(buf, sizeof(buf), "Get4:0x%02x Epoche2:0x%08x StampTime:0x%x Sync:%x Dataloss:%x Epochloss:%x Epochmissmatch:%x", 
                     getEpoch2ChipNumber(), getEpoch2Number(), getEpoch2StampTime(), getEpoch2Sync(),
                     getEpoch2DataLost(), getEpoch2EpochLost(), getEpoch2EpochMissmatch());
            break;
         case MSG_GET4:
            snprintf(buf, sizeof(buf), "Get4:0x%02x Chn:%1x Edge:%1x Ts:0x%05x CRC8:0x%02x",
                  getGdpbHitChipId(), getGdpbHitChanId(), getGdpbHit24Edge(), getGdpbHitFullTs(), getGdpbHitCrc() );
            break;
         case MSG_SYS: {
            char sysbuf[256];

            switch (getSysMesType()) {
              case SYSMSG_DAQ_START:
                 snprintf(sysbuf, sizeof(sysbuf), "DAQ started");
                 break;
              case SYSMSG_DAQ_FINISH:
                 snprintf(sysbuf, sizeof(sysbuf), "DAQ finished");
                 break;
              case SYSMSG_NX_PARITY: {
                   uint32_t nxb3_data = getSysMesData();
                   uint32_t nxb3_flg = (nxb3_data>>31) & 0x01;
                   uint32_t nxb3_val = (nxb3_data>>24) & 0x7f;
                   uint32_t nxb2_flg = (nxb3_data>>23) & 0x01;
                   uint32_t nxb2_val = (nxb3_data>>16) & 0x7f;
                   uint32_t nxb1_flg = (nxb3_data>>15) & 0x01;
                   uint32_t nxb1_val = (nxb3_data>>8 ) & 0x7f;
                   uint32_t nxb0_flg = (nxb3_data>>7 ) & 0x01;
                   uint32_t nxb0_val = (nxb3_data    ) & 0x7f;
                   snprintf(sysbuf, sizeof(sysbuf),"Nx:%1x %1d%1d%1d%1d:%02x:%02x:%02x:%02x nX parity error", getNxNumber(), 
                            nxb3_flg, nxb2_flg, nxb1_flg, nxb0_flg,
                            nxb3_val, nxb2_val, nxb1_val, nxb0_val);
                 }
                 break;
              case SYSMSG_SYNC_PARITY:
                 snprintf(sysbuf, sizeof(sysbuf), "SYNC parity error ");
                 break;
              case SYSMSG_DAQ_RESUME:
                 snprintf(sysbuf, sizeof(sysbuf), "DAQ resume after high/low water");
                 break;
              case SYSMSG_FIFO_RESET:
                 snprintf(sysbuf, sizeof(sysbuf), "FIFO reset");
                 break;
              case SYSMSG_USER: {
                 const char* subtyp = "";
                 if (getSysMesData()==SYSMSG_USER_CALIBR_ON) subtyp = "Calibration ON"; else
                 if (getSysMesData()==SYSMSG_USER_CALIBR_OFF) subtyp = "Calibration OFF"; else
                 if (getSysMesData()==SYSMSG_USER_RECONFIGURE) subtyp = "Reconfigure";
                 snprintf(sysbuf, sizeof(sysbuf), "User message 0x%08x %s", getSysMesData(), subtyp);
                 break;
              }
              case SYSMSG_PACKETLOST:
                 snprintf(sysbuf, sizeof(sysbuf), "Packet lost");
                 break;
              case SYSMSG_GET4_EVENT:
              {
                 break;
              } //
              case SYSMSG_CLOSYSYNC_ERROR:
                 snprintf(sysbuf, sizeof(sysbuf), "Closy synchronization error");
                 break;
              case SYSMSG_TS156_SYNC:
                 snprintf(sysbuf, sizeof(sysbuf), "156.25MHz timestamp reset");
                 break;
              case SYSMSG_GET4V1_32BIT_0:
              case SYSMSG_GET4V1_32BIT_1:
              case SYSMSG_GET4V1_32BIT_2:
              case SYSMSG_GET4V1_32BIT_3:
              case SYSMSG_GET4V1_32BIT_4:
              case SYSMSG_GET4V1_32BIT_5:
              case SYSMSG_GET4V1_32BIT_6:
              case SYSMSG_GET4V1_32BIT_7:
              case SYSMSG_GET4V1_32BIT_8:
              case SYSMSG_GET4V1_32BIT_9:
              case SYSMSG_GET4V1_32BIT_10:
              case SYSMSG_GET4V1_32BIT_11:
              case SYSMSG_GET4V1_32BIT_12:
              case SYSMSG_GET4V1_32BIT_13:
              case SYSMSG_GET4V1_32BIT_14:
              case SYSMSG_GET4V1_32BIT_15:
              {
                 break;
              } // case SYSMSG_GET4V1_32BIT + channel!
              default:
                 snprintf(sysbuf, sizeof(sysbuf), "unknown system message type ");
            } // switch (getSysMesType())

            snprintf(buf, sizeof(buf), "SysType:%2x Data:%8x : %s", getSysMesType(), getSysMesData(), sysbuf);

            break;
         } // case MSG_SYS:
         case MSG_GET4_SLC:
         {
            // GET4 slow control message, new "true" ROC support
            snprintf(buf, sizeof(buf),
               "Get4 Slow control, chip %02d => Chan:%01d Edge:%01d Type:%01x Data:0x%06x CRC:0x%02x",
               getGdpbSlcChipId(), 0x0, 0x0, 0x0, getGdpbSlcData(), getGdpbSlcCrc() );
            break;
         } // case MSG_GET4_SLC:
         case MSG_GET4_32B:
         {
            // 32b GET4 data event, new "true" ROC support
            snprintf(buf, sizeof(buf),
              "Get4 32 bits, Chip:0x%02x Dll %1d Channel %1d Ts:0x%03x Ft:0x%02x Tot:0x%02x",
              getGdpbHitChipId(), getGdpbHit32DllLck(), getGdpbHitChanId(),
              getGdpbHitCoarse(), getGdpbHitFineTs(), getGdpbHit32Tot() );

            break;
         } // case MSG_GET4_32B:
         case MSG_GET4_SYS:
         {
            // GET4 system message, new "true" ROC support
            char sysbuf[256];

            switch( getGdpbSysSubType() )
            {
               case SYSMSG_GET4_EVENT:
               {
                 snprintf(sysbuf, sizeof(sysbuf),
                   "Get4:0x%02x Ch:0x%01x Edge:%01x Unused:%06x ErrCode:0x%02x - GET4 V1 Error Event",
                   getGdpbSysChipId(), getGdpbSysErrChanId(), getGdpbSysErrEdge(), getGdpbSysErrUnused(), getGdpbSysErrData());
                  break;
               } //
               case SYSMSG_CLOSYSYNC_ERROR:
                  snprintf(sysbuf, sizeof(sysbuf), "Closy synchronization error");
                  break;
               case SYSMSG_TS156_SYNC:
                  snprintf(sysbuf, sizeof(sysbuf), "156.25MHz timestamp reset");
                  break;
               case SYSMSG_GDPB_UNKWN:
                  snprintf(sysbuf, sizeof(sysbuf), "Unknown GET4 message, data: 0x%08x", getGdpbSysUnkwData());
                  break;
            } // switch( getGdpbSysSubType() )
            break;
         } // case MSG_GET4_SYS:
         default:
           snprintf(buf, sizeof(buf), "Error - unexpected MessageType: %1x", getMessageType());
      }
   }

//   os << buf << std::endl;
   if( msg_print_Cout == outType)
      std::cout << buf << std::endl;
   else if( msg_print_File == outType )
      os << buf << std::endl;
//   else LOG(INFO) << sLogBuff << buf << FairLogger::endl;
}

uint32_t ngdpb::Message::RawSize(int fmt)
{
   switch (fmt) {
      case formatNormal: return 8;
      default:
         std::cerr << "ngdpb::Message::RawSize => "
                   << " Deprecated format, nothing done!!"
                   << std::endl;
         return 0;
   }
   return 8;
}

bool ngdpb::Message::assign(void* src, int fmt)
{
   switch (fmt) {
      case formatNormal:
         memcpy(&data, src, 8);
         return true;
      default:
         std::cerr << "ngdpb::Message::assign => "
                   << " Deprecated format, nothing done!!"
                   << std::endl;
         return false;
   }

   return false;
}

bool ngdpb::Message::copyto(void* tgt, int fmt)
{
   switch (fmt) {
      case formatNormal:
         memcpy(tgt, &data, 8);
         return true;
      default:
         std::cerr << "ngdpb::Message::assign => "
                   << " Deprecated format, nothing done!!"
                   << std::endl;
         return false;
   }

   return false;

}

/*
double ngdpb::Message::CalcGet4V10R24HitTimeDiff(
      uint32_t epochA, uint32_t epochB, Message& messB)
{
   Double_t dTimeDiff = 0.0;
   // if conditions needed to deal with unsigned int at full resolution
   if( epochA < epochB)
      dTimeDiff -=
            ( epochB - epochA )*get4v1x::kdEpochInPs;
      else dTimeDiff +=
            ( epochA - epochB )*get4v1x::kdEpochInPs;
   // if conditions needed to deal with unsigned int at full resolution
   if( this->getGet4Ts() < messB.getGet4Ts() )
      dTimeDiff -=
            ( messB.getGet4Ts() - this->getGet4Ts()
            )*get4v1x::kdBinSize;
      else dTimeDiff +=
            ( this->getGet4Ts() - messB.getGet4Ts()
            )*get4v1x::kdBinSize;

   return dTimeDiff;
}

double ngdpb::Message::CalcGet4V10R32HitTimeDiff(
      uint32_t epochA, uint32_t epochB, Message& messB)
{
   Double_t dTimeDiff = 0.0;
   // if conditions needed to deal with unsigned int at full resolution
   if( epochA < epochB)
      dTimeDiff -=
            ( epochB - epochA )*get4v1x::kdEpochInPs;
      else dTimeDiff +=
            ( epochA - epochB )*get4v1x::kdEpochInPs;
   // if conditions needed to deal with unsigned int at full resolution
   if( this->getGet4V10R32HitTimeBin() < messB.getGet4V10R32HitTimeBin() )
      dTimeDiff -=
            ( messB.getGet4V10R32HitTimeBin() - this->getGet4V10R32HitTimeBin()
            )*get4v1x::kdBinSize;
      else dTimeDiff +=
            ( this->getGet4V10R32HitTimeBin() - messB.getGet4V10R32HitTimeBin()
            )*get4v1x::kdBinSize;

   return dTimeDiff;
}

//----------------------------------------------------------------------------
//! strict weak ordering operator, including epoch for both messages
bool ngdpb::FullMessage::operator<(const FullMessage& other) const
{
   if( other.fuExtendedEpoch == this->fuExtendedEpoch )
      // Same epoch => use Message (base) class ordering operator
      return this->Message::operator<( other );
      else return this->fuExtendedEpoch < other.fuExtendedEpoch;

}
*/
