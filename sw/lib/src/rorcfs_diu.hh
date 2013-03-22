/**
 * @file rorcfs_diu.hh
 * @author Heiko Engel <hengel@cern.ch>
 * @version 0.1
 * @date 2011-09-27
 * 
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef _RORCLIB_RORCFS_DIU_H
#define _RORCLIB_RORCFS_DIU_H


/** GTX PHY is enabled */
#define GTX_PHY_ENABLED (1<<0)
/** GTX PHY is ready */
#define GTX_PHY_READY (1<<1)
/** GTX PHY is in loopback mode */
#define GTX_PHY_LOOPEN (1<<2)
/** GTX RX Byte is aligned */
#define GTX_RXBYTEISALIGNED (1<<3)
/** GTX RX PLL Link detected */
#define GTX_RXPLLLKDET (1<<4)
/** GTX TX PLL Link detected */
#define GTX_TXPLLLKDET (1<<5)
/** GTX TX reset done */
#define GTX_TXRESETDONE (1<<6)
/** GTX RX reset done */
#define GTX_RXRESETDONE (1<<7)
/** GTX_TX reset active */
#define GTX_TXRESET (1<<8)
/** GTX RX reset active */
#define GTX_RXRESET (1<<9)
/** GTX global reset active */
#define GTX_GTXRESET (1<<10)
/** GTX electrically idle / OOB */
#define GTX_RXELECIDLE (1<<11)
/** GTX clock stopped */
#define GTX_CLK_STOPPED (1<<12)
/** GTX Loss of Sync */
#define GTX_RXLOSSOFSYNC (1<<13)



/**
 * @class rorcfs_diu
 * @brief DIU Interface class
 *
 * Attach an instance of rorcfs_diu to an existing
 * DMA channel instance. This gives access to the DIU
 * Interface status registers and allows to attach an
 * actual DIU PHY or a pattern generator
 **/
class rorcfs_diu
{
	public:
		rorcfs_diu();
		~rorcfs_diu();

		/**
		 * initialize DIU Interface
		 * @param ch parent rorcfs_dma_channel
		 * @return 0 on sucess, -1 on errors
		 **/
		int init( rorcfs_dma_channel *ch );

		/**
		 * get DIU enable
		 * @return 1 if enabled, 0 if disabled
		 **/
//		unsigned int getDiuEnable();
		
		/**
		 * set DIU enable
		 *
		 * DIU enable controls roRST_N. As long as
		 * the DIUEnable is zero the DIU is held in reset
		 * state. Any slow control interface on DIU_IF is
		 * held in reset as long as DIUEnable is not set.
		 * @param enable 1 to enable, 0 to disable
		 **/
//		void setDiuEnable( int enable );

		/**
		 * get last sent DIU Command. This is only valid when
		 * DIU is enabled.
		 * @return last DIU Command
		 **/
//		unsigned int getCmd();

		/**
		 * send DIU Command. 
		 * @param cmd DIU Command
		 **/
//		void setCmd( unsigned int cmd );

		/**
		 * get last DIU Status. This is only valid when
		 * DIU is enabled.
		 * @return DIU Status
		 **/
//		unsigned int getStatus();

		/**
		 * clear DIU Status Register. This is only valid when
		 * DIU is enabled.
		 **/
//		void clearStatus();

		/**
		 * get DIU Interface Deadtime (the time link_full_N is low). 
		 * This is only valid when DIU is enabled.
		 * @return DIU Deadtime in units of DIU_CLK periods
		 **/
//		unsigned int getIfDeadtime();

		/**
		 * clear DIU Deadtime Counter
		 **/
//		void clearIfDeadtime();

		/**
		 * get DIU Event Count
		 * This is only valid when DIU is enabled.
		 * @return number of received events since the last
		 * reset or last clearEventCount()
		 **/
//		unsigned int getIfEventCount();

		/**
		 * clear DIU Event Count
		 **/
//		void clearIfEventCount();

		/**
		 * get DIU Interface Status:
		 * This is only valid when DIU is enabled.
		 *
		 * @return DIU Interface Status:
		 * status[31] = riLD_N
		 * status[30] = riLF_N
		 * status[29] = link_full_N
		 * status[28:1] = 0
		 * status[1] = DIU_enable
		 * status[0] = busy_flag
		 **/
//		unsigned int getIfStatus();


		/**
		 * set DIU IF config
		 * config[1] = DIU_enable
		 * config[0] = busy_flag
		 * @param config configuration vector
		 **/
//		void setIfConfig( unsigned int config );

		/**
		 * set DIU Interface Busy
		 *
		 * when enabled (set 1) the DIU generates a roBSY_N signal to
		 * stop the link from receiving when link is full
		 *
		 * when disabled (set 0, default) the DIU will never issue a
		 * link_full_N, thus a roBSY_N
		 *
		 * \note you will most likely want to enable DIU IF busy
		 *
		 * @param busy valid values are 0 or 1
		 **/
//		void setIfBusy( unsigned int busy );

		/**
		 * get last calculated event length
		 * This is only valid when DIU is enabled.
		 * @return Calculated Event Length
		 **/
//		unsigned int getIfEventLengthCalc();

		/**
		 * clear calculated event length counter
		 *
		 * this has priority over incrementing the data count
		 * on valid data words - Don't use unless you know what 
		 * you're doing!
		 **/
//		void clearIfEventLengthCounter();

		/**
		 * get GTX status flags
		 * @return wired-OR of GTX_* defines in rorcfs_diu.hh
		 **/
//		unsigned int getGtxStatusFlags();

		/**
		 * get GTX loopback settings
		 * @return 3 bit loopback status
		 * 000: normal operation
		 * 001 Near-End PCS
		 * 010: Near-End PMA Loopback
		 * 011: Reserved
		 * 100: Far-End PMA Loopback
		 * 101: Reserved
		 * 110: Far-End PCS Loopback
		 **/
//		unsigned int getGtxLoopback();

		/**
		 * get GTX Configurable Driver Status
		 * @return 4bit driver swing control value
		 * see UG366, p175 for values
		 **/
//		unsigned int getGtxTxDiffCtrl();

		/**
		 * get GTX Transmitter Pre-Cursor TX Pre-Emphasis
		 * @return emphasis
		 * see UG366, p177 for values
		 **/
		unsigned int getGtxTxPreEmph();

		/**
		 * get GTX Transmitter Post-Cursor TX Pre-Emphasis
		 * @return  emphasis
		 * see UG366, p176 for values
		 **/
//		unsigned int getGtxPostEmph();

		/**
		 * get GTX clock correction status of the RX elastic buffer
		 * @return 3 bit correction, -1 on DIU clk down
		 * 000: No clock correction
		 * 001: 1 sequence skipped
		 * 010: 2 sequences skipped
		 * 011: 3 sequences skipped
		 * 100: 4 sequences skipped
		 * 101: Reserved
		 * 110: 2 sequences added
		 * 111: 1 sequence added
		 **/
//		int getGtxRxClkCorCnt();

		/**
		 * get GTX RX buffer status
		 * @return 3 bit status, -1 on DIU clk down
		 * 000: Nominal condition
		 * 001: Number of bytes in the buffer are less than CLK_COR_MIN_LAT
		 * 010: Number of bytes in the buffer are greater than CLK_COR_MAX_LAT
		 * 101: RX elastic buffer underflow
		 * 110: RX elastic buffer overflow
		 **/
//		int getGtxRxBufStatus();

		/**
		 * get GTX TX buffer status
		 * @return 2 bit status, -1 on DIU clk down
		 * TXBUFSTATUS[1]: TX buffer overflow or underflow
		 * 1: FIFO has overflowed or underflowed
		 * 0: No overflow/underflow error
		 * TXBUFSTATUS[0]: TX buffer fullness
		 * 1: FIFO is at least half full
		 * 0: FIFO is less than half full
		 * When TXBUFSTATUS[1] goes High, it remains High 
		 * until TXRESET is asserted.
		 **/
//		int getGtxTxBufStatus();

		/**
		 * set GTX Loopback mode
		 * @param value
		 * 000: normal operation
		 * 001 Near-End PCS
		 * 010: Near-End PMA Loopback
		 * 011: Reserved
		 * 100: Far-End PMA Loopback
		 * 101: Reserved
		 * 110: Far-End PCS Loopback
		 **/
//		void setGtxLoopback(unsigned int value);

		/**
		 * set GTX Configurable Driver Control
		 * @param value see UG366, p175 for valid values
		 **/
//		void setGtxTxDiffCtrl(unsigned int value);

		/**
		 * set GTX Transmitter Pre-Cursor TX Pre-Emphasis
		 * @param value see UG366, p177 for values
		 **/
//		void setGtxTxPreEmph(unsigned int value);

		/**
		 * set GTX Transmitter Post-Cursor TX Pre-Emphasis
		 * @param value see UG366, p176 for values
		 **/
//		void setGtxTxPostEmph(unsigned int value);

		/**
		 * get GTX Clocking Configuration
		 * @return value[31:0] = 4'b0 & 8'bCLK25DIV &
		 * 4'bCLKN1 & 4'bCLKN2 & 4'bCLKD & 4'bCLKM
		 **/
//		unsigned int getGtxClkCfg();

		/**
		 * get underlying mmap'ed BAR
		 * @return pointer to mmap'ed BAR
		 **/
		rorcfs_bar *getBar() {
			return bar;
		}

		/**
		 * get base address of DMA channel
		 * in its BAR
		 * @return unsigned int base address
		 **/
		unsigned int getBase() {
			return base;
		}

	private:
		rorcfs_bar *bar;
		unsigned int base;
};

#endif
