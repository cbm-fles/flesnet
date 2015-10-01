// rorc_registers.h: RORC Register Definitions
//
// manually composed register file
// for cnet readout and flesin design
#define RORC_C_BUILD_REPO 1
#define RORC_C_BUILD_DATE 1417530947
#define RORC_C_BUILD_REVISION 33f97ce547f05b49ba0d90de62cda8ea8e2e421b
#define RORC_C_BUILD_STATUS_CLEAN 1
#define RORC_C_BUILD_HOST 65646100000000000000000000000000
#define RORC_C_BUILD_USER 68757474657200000000000000000000
#define RORC_C_HARDWARE_VERSION 8
#define RORC_CHANNEL_OFFSET 0x00008000
#define RORC_DMA_CMP_SEL 13
#define RORC_REG_HARDWARE_INFO 0
#define RORC_REG_BUILD_FLAGS 1
#define RORC_REG_N_CHANNELS 2
#define RORC_REG_SC_REQ_CANCELED 3
#define RORC_REG_DMA_TX_TIMEOUT 4
#define RORC_REG_ILLEGAL_REQ 5
#define RORC_REG_MULTIDWREAD 6
#define RORC_REG_PCIE_CTRL 7
#define RORC_REG_PCIE_DST_BUSY 8
#define RORC_REG_MC_CNT_CFG 9
#define RORC_REG_DLM_CFG 10 // cnet only
#define RORC_REG_APP_CFG 11
#define RORC_REG_APP_STS 12
#define RORC_REG_BUILD_DATE_L 13
#define RORC_REG_BUILD_DATE_H 14
#define RORC_REG_BUILD_REV_0 15
#define RORC_REG_BUILD_REV_1 16
#define RORC_REG_BUILD_REV_2 17
#define RORC_REG_BUILD_REV_3 18
#define RORC_REG_BUILD_REV_4 19
#define RORC_REG_BUILD_HOST_0 20
#define RORC_REG_BUILD_HOST_1 21
#define RORC_REG_BUILD_HOST_2 22
#define RORC_REG_BUILD_HOST_3 23
#define RORC_REG_BUILD_USER_0 24
#define RORC_REG_BUILD_USER_1 25
#define RORC_REG_BUILD_USER_2 26
#define RORC_REG_BUILD_USER_3 27
#define RORC_REG_EBDM_N_SG_CONFIG 0
#define RORC_REG_EBDM_BUFFER_SIZE_L 1
#define RORC_REG_EBDM_BUFFER_SIZE_H 2
#define RORC_REG_RBDM_N_SG_CONFIG 3
#define RORC_REG_RBDM_BUFFER_SIZE_L 4
#define RORC_REG_RBDM_BUFFER_SIZE_H 5
#define RORC_REG_EBDM_SW_READ_POINTER_L 6
#define RORC_REG_EBDM_SW_READ_POINTER_H 7
#define RORC_REG_RBDM_SW_READ_POINTER_L 8
#define RORC_REG_RBDM_SW_READ_POINTER_H 9
#define RORC_REG_DMA_CTRL 10
#define RORC_REG_DMA_N_EVENTS_PROCESSED 11
#define RORC_REG_EBDM_FPGA_WRITE_POINTER_H 12
#define RORC_REG_EBDM_FPGA_WRITE_POINTER_L 13
#define RORC_REG_RBDM_FPGA_WRITE_POINTER_L 14
#define RORC_REG_RBDM_FPGA_WRITE_POINTER_H 15
#define RORC_REG_SGENTRY_ADDR_LOW 16
#define RORC_REG_SGENTRY_ADDR_HIGH 17
#define RORC_REG_SGENTRY_LEN 18
#define RORC_REG_SGENTRY_CTRL 19
#define RORC_REG_DMA_STALL_CNT 20
#define RORC_REG_MISC_CFG 21
#define RORC_REG_MISC_STS 22
#define RORC_REG_EBDM_OFFSET_H 23
#define RORC_REG_EBDM_OFFSET_L 24
#define RORC_REG_GTX_DATAPATH_CFG 0
#define RORC_REG_GTX_DATAPATH_STS 1
#define RORC_REG_GTX_MC_GEN_CFG 2
#define RORC_REG_GTX_PENDING_MC_L 3
#define RORC_REG_GTX_PENDING_MC_H 4
#define RORC_REG_GTX_CTRL_TX 5 // cnet only
#define RORC_REG_GTX_CTRL_RX 6 // cnet only
#define RORC_REG_GTX_DLM 7     // cnet only
#define RORC_REG_GTX_MC_GEN_CFG_HDR 8
#define RORC_REG_GTX_MC_GEN_CFG_IDX_L 9
#define RORC_REG_GTX_MC_GEN_CFG_IDX_H 10
#define RORC_REG_GTX_MC_INDEX_L 11
#define RORC_REG_GTX_MC_INDEX_H 12
#define RORC_MEM_BASE_CTRL_TX 16           // cnet only
#define RORC_MEM_SIZE_CTRL_TX 16           // cnet only
#define RORC_MEM_BASE_CTRL_RX 32           // cnet only
#define RORC_MEM_SIZE_CTRL_RX 16           // cnet only
#define RORC_REG_GTX_DIAG_PCS_STARTUP 48   // cnet only
#define RORC_REG_GTX_DIAG_EBTB_CODE_ERR 49 // cnet only
#define RORC_REG_GTX_DIAG_EBTB_DISP_ERR 50 // cnet only
#define RORC_REG_GTX_DIAG_CRC_ERROR 51     // cnet only
#define RORC_REG_GTX_DIAG_PACKET 52        // cnet only
#define RORC_REG_GTX_DIAG_PACKET_ERR 53    // cnet only
#define RORC_REG_GTX_DIAG_FLAGS 54         // cnet only
#define RORC_REG_GTX_DIAG_CLEAR 55         // cnet only
#define RORC_REG_LINK_FLIM_HW_INFO 0         // flesin only
#define RORC_REG_FLIM_BUILD_FLAGS 1          // flesin only
#define RORC_REG_FLIM_BUILD_DATE_L 2         // flesin only
#define RORC_REG_FLIM_BUILD_DATE_H 3         // flesin only
#define RORC_REG_FLIM_BUILD_REV_0 4          // flesin only
#define RORC_REG_FLIM_BUILD_REV_1 5          // flesin only
#define RORC_REG_FLIM_BUILD_REV_2 6          // flesin only
#define RORC_REG_FLIM_BUILD_REV_3 7          // flesin only
#define RORC_REG_FLIM_BUILD_REV_4 8          // flesin only
#define RORC_REG_FLIM_BUILD_HOST_0 9         // flesin only
#define RORC_REG_FLIM_BUILD_HOST_1 10        // flesin only
#define RORC_REG_FLIM_BUILD_HOST_2 11        // flesin only
#define RORC_REG_FLIM_BUILD_HOST_3 12        // flesin only
#define RORC_REG_FLIM_BUILD_USER_0 13        // flesin only
#define RORC_REG_FLIM_BUILD_USER_1 14        // flesin only
#define RORC_REG_FLIM_BUILD_USER_2 15        // flesin only
#define RORC_REG_FLIM_BUILD_USER_3 16        // flesin only
#define RORC_REG_LINK_FLIM_CFG 17            // flesin only
#define RORC_REG_LINK_FLIM_STS 18            // flesin only
#define RORC_REG_LINK_MC_INDEX_L 19          // flesin only
#define RORC_REG_LINK_MC_INDEX_H 20          // flesin only
#define RORC_REG_LINK_MC_PACKER_CFG_IDX_L 21 // flesin only
#define RORC_REG_LINK_MC_PACKER_CFG_IDX_H 22 // flesin only
#define RORC_REG_LINK_MC_PGEN_CFG 23         // flesin only
#define RORC_REG_LINK_MC_PGEN_MC_SIZE 24     // flesin only
#define RORC_REG_LINK_MC_PGEN_IDS 25         // flesin only
#define RORC_REG_LINK_MC_PGEN_MC_PENDING 26  // flesin only
#define RORC_REG_LINK_TEST 31                // flesin only
