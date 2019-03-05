// Copyright 2013-2015 Jan de Cuveland <cmail@cuveland.de>

#include "NgdpbDebugger.hpp"
#include <algorithm>
#include <boost/format.hpp>

#include "rocMess_wGet4v1.h"

#include <iomanip>

std::ostream& NgdpbDump::write_to_stream(std::ostream& s) const {
  constexpr int bytes_per_block = 8;
  //    constexpr int bytes_per_line = 4 * bytes_per_block;
  int iNbMessages = (size - (size % bytes_per_block)) / bytes_per_block;
  std::cout << "-----------------------------  " << iNbMessages << std::endl;

  for (int i = 0; i < static_cast<int>(iNbMessages); i++) {
    uint64_t ulData =
        static_cast<uint64_t>(static_cast<const uint64_t*>(buf)[i]);

    ngdpb::Message mess(ulData);

    std::cout << std::hex << std::setw(8) << ulData << " " << std::dec;

    mess.printData(ngdpb::msg_print_File,
                   ngdpb::msg_print_Hex | ngdpb::msg_print_Prefix |
                       ngdpb::msg_print_Data,
                   0, s);
    /*
            // First dump the DPB identifier
            s << boost::format("DPB: %04x ") % getField(ulData, 48, 16);

            // Then dump the hexadecimal content
            s << boost::format("%02X:%02X:%02X:%02X:%02X:%02X ") %
                   getField(ulData, 40, 8) % getField(ulData, 32, 8) %
       getField(ulData, 24, 8) % getField(ulData, 16, 8) % getField(ulData,  8,
       8) % getField(ulData,  0, 8);

            // Then dump formatted message
            switch ( getField(ulData, 0, 4) ) {
                case MSG_NOP:
                   s << boost::format("NOP (raw %02X:%02X:%02X:%02X:%02X:%02X)")
       % getField(ulData, 40, 8) % getField(ulData, 32, 8) % getField(ulData,
       24, 8) % getField(ulData, 16, 8) % getField(ulData,  8, 8) %
       getField(ulData,  0, 8); break; case MSG_HIT: s << boost::format("Nx:%1x
       Chn:%02x Ts:%04x Last:%1x Msb:%1x Adc:%03x Pup:%1x Oflw:%1x") %
                           getField(ulData,6, 2) % getField(ulData,25,  7) %
       getField(ulData,11, 14) % getBit(ulData,47) % getField(ulData,8, 3) %
       getField(ulData,33, 12) % getBit(ulData,45) %       getBit(ulData,46);
                   break;
                case MSG_EPOCH:
                   s << boost::format("Epoch:%08x Missed:%02x") %
                           getField(ulData,8, 32) % getField(ulData,40, 8);
                   break;
                case MSG_SYNC:
                   s << boost::format("SyncChn:%1x EpochLSB:%1x Ts:%04x
       Data:%06x Flag:%1x") % getField(ulData, 6,  2) % getBit(ulData,21) %
       (getField(ulData,8, 13) << 1) % getField(ulData,22, 24) %
       getField(ulData,46, 2); break; case MSG_AUX: s <<
       boost::format("AuxChn:%02x EpochLSB:%1x Ts:%04x Falling:%1x
       Overflow:%1x") % getField(ulData,6, 7) % getBit(ulData,26) %
       (getField(ulData,13, 13) << 1) % getBit(ulData,27) % getBit(ulData,28);
                   break;
                case MSG_EPOCH2:
                   s << boost::format("Get4:0x%02x Epoche2:0x%08x StampTime:0x%x
       Sync:%x Dataloss:%x Epochloss:%x Epochmissmatch:%x") % getBit(ulData,4) %
       getField(ulData,10, 32) % getField(ulData,8, 2) % getBit(ulData,7) %
                           getBit(ulData,6) % getBit(ulData,5) %
       getBit(ulData,4); break; case MSG_GET4: s << boost::format("Get4:0x%02x
       Chn:%1x Edge:%1x Ts:0x%05x CRC8:0x%02x") % getField(ulData,6, 6) %
       getField(ulData,12, 2) % getBit(ulData,33) % getField(ulData,14, 19) %
       getField(ulData,40, 8); break; case MSG_SYS: switch ( getField(ulData,8,
       8) ) { case SYSMSG_NX_PARITY: { uint32_t nxb3_data = getField(ulData,16,
       32); uint32_t nxb3_flg = (nxb3_data>>31) & 0x01; uint32_t nxb3_val =
       (nxb3_data>>24) & 0x7f; uint32_t nxb2_flg = (nxb3_data>>23) & 0x01;
                          uint32_t nxb2_val = (nxb3_data>>16) & 0x7f;
                          uint32_t nxb1_flg = (nxb3_data>>15) & 0x01;
                          uint32_t nxb1_val = (nxb3_data>>8 ) & 0x7f;
                          uint32_t nxb0_flg = (nxb3_data>>7 ) & 0x01;
                          uint32_t nxb0_val = (nxb3_data    ) & 0x7f;

                          s << boost::format("Nx:%02x
       %1d%1d%1d%1d:%02x:%02x:%02x:%02x nX parity error") % getField(ulData, 6,
       2) % nxb3_flg % nxb2_flg % nxb1_flg % nxb0_flg % nxb3_val % nxb2_val %
       nxb1_val % nxb0_val;
                        }
                        break;
                     case SYSMSG_SYNC_PARITY:
                        s << "SYNC parity error ";
                        break;
                     case SYSMSG_FIFO_RESET:
                        s << "FIFO reset";
                        break;
                     case SYSMSG_USER: {
                        const char* subtyp = "";
                        if (getField(ulData,16, 32)==SYSMSG_USER_CALIBR_ON)
       subtyp = "Calibration ON"; else if (getField(ulData,16,
       32)==SYSMSG_USER_CALIBR_OFF) subtyp = "Calibration OFF"; else if
       (getField(ulData,16, 32)==SYSMSG_USER_RECONFIGURE) subtyp =
       "Reconfigure"; s << boost::format("User message 0x%08x %s") %
                               getField(ulData,16, 32) % subtyp;
                        break;
                     }
                     case SYSMSG_PACKETLOST:
                        s << "Packet lost";
                        break;
                     case SYSMSG_GET4_EVENT:
                     {
                        // 24b GET4 Error event
                        uint32_t get4_24b_er_chan = getField(ulData, 26, 2);
                        uint32_t get4_24b_er_edge = getBit(ulData,   28);
                        uint32_t get4_24b_er_unus = getField(ulData, 29, 6);
                        uint32_t get4_24b_er_chip = getField(ulData, 40, 8);
                        uint32_t get4_24b_er_code = getField(ulData, 16, 10);

                        s << boost::format("Get4:0x%02x Ch:0x%01x Edge:%01x
       Unused:%04x ErrCode:0x%02x - GET4 V1 Error Event")  % get4_24b_er_chip %
       get4_24b_er_chan % get4_24b_er_edge % get4_24b_er_unus %
       get4_24b_er_code; break; } // case SYSMSG_CLOSYSYNC_ERROR: s << "Closy
       synchronization error"; break; case SYSMSG_TS156_SYNC: s << "156.25MHz
       timestamp reset"; break;
                   }
                   break;
                default:
                   s << boost::format("UNK (raw %02X:%02X:%02X:%02X:%02X:%02X)")
       % getField(ulData, 40, 8) % getField(ulData, 32, 8) % getField(ulData,
       24, 8) % getField(ulData, 16, 8) % getField(ulData,  8, 8) %
       getField(ulData,  0, 8); break;
            }
            s << "\n";
    */
  } // for (int i = 0; i < static_cast<int>(iNbMessages); i ++)

  return s;
}
