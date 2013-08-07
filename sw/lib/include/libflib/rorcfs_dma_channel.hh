/**
 * @file rorcfs_dma_channel.hh
 * @author Heiko Engel <hengel@cern.ch>
 * @version 0.1
 * @date 2011-08-17
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

#ifndef _RORCLIB_RORCFS_DMA_CHANNEL_H
#define _RORCLIB_RORCFS_DMA_CHANNEL_H

/** default maximum payload size in bytes. Check the capabilities
 * of the chipset and the FPGA PCIe core before modifying this value
 * Common values are 128 or 256 bytes.*/
#define MAX_PAYLOAD 128


/** struct holding both read pointers and the
 * DMA engine configuration register contents **/
struct rorcfs_buffer_software_pointers {
	/** EBDM read pointer low **/
	uint32_t ebdm_software_read_pointer_low; 
	/** EBDM read pointer high **/
	uint32_t ebdm_software_read_pointer_high;
	/** RBDM read pointer low **/
	uint32_t rbdm_software_read_pointer_low;
	/** RBDM read pointer high **/
	uint32_t rbdm_software_read_pointer_high;
	/** DMA control register **/
	uint32_t dma_ctrl;
};

/** struct rorcfs_channel_config **/
struct rorcfs_channel_config {
	/** EBDM number of sg entries **/
	uint32_t ebdm_n_sg_config;
	/** EBDM buffer size low (in bytes) **/
	uint32_t ebdm_buffer_size_low;
	/** EBDM buffer size high (in bytes) **/
	uint32_t ebdm_buffer_size_high;
	/** RBDM number of sg entries **/
	uint32_t rbdm_n_sg_config;
	/** RBDM buffer size low (in bytes) **/
	uint32_t rbdm_buffer_size_low;
	/** RBDM buffer size high (in bytes) **/
	uint32_t rbdm_buffer_size_high;
	/** struct for read pointers nad control register **/
	struct rorcfs_buffer_software_pointers swptrs;
};

/** struct t_sg_entry_cfg **/
struct t_sg_entry_cfg {
	/** lower part of sg address **/
	uint32_t sg_addr_low;
	/** higher part of sg address **/
	uint32_t sg_addr_high;
	/** total length of sg entry in bytes **/
	uint32_t sg_len;
	/** BDM control register: [31]:we, [30]:sel, [29:0]BRAM addr **/
	uint32_t ctrl;
};


/**
 * @class rorcfs_dma_channel
 * @brief DMA channel management class
 *
 * Initialize DMA channel with init() before using any other member
 * function. Initialization sets the parent BAR and the channel offset 
 * within the BAR (via dma_base). Use prepareEB() and prepareRB() to 
 * fill the buffer descriptor memories, then configure Buffer-Enable
 * and -Continuous flags (setRBDMEnable(), setEBDMEnable(), 
 * setRBDMContinuous(), setEBDMContinuous() ). 
 * No DMA transfer will start unless setDMAEnable() has been called 
 * with enable=1.
 **/
class rorcfs_dma_channel
{
	public:
		rorcfs_dma_channel();
		~rorcfs_dma_channel();

		/**
		 * initialize DMA base address within BAR
		 * @param dma_base unsigned int base address
		 * @param dma_bar according instance of rorcfs_bar
		 **/
		void init(rorcfs_bar *dma_bar, unsigned int dma_base);

		/**
		 * prepare EventBuffer: copy scatterlist from 
		 * rorcfs_buffer into the EventBufferDescriptorManager
		 * in the RORC
		 * @param buf rorcfs_buffer instance to be used as
		 * 						event destination buffer
		 * @return 0 on sucess, -1 on errors, -EFBIG if more
		 * 				 than 2048 sg-entries
		 **/
		int prepareEB ( rorcfs_buffer *buf );

		/**
		 * prepare ReportBuffer: copy scatterlist from 
		 * rorcfs_buffer into the ReportBufferDescriptorManager
		 * in the RORC
		 * @param buf rorcfs_buffer instance to be used as
		 * 						report destination buffer
		 * @return 0 on sucess, -1 on errors
		 **/
		int prepareRB ( rorcfs_buffer *buf );

		/**
		 * set Enable Bit of EBDM
		 * @param enable nonzero param will enable, zero will disable
		 **/
		void setEnableEB( int enable );

		/**
		 * set Enable Bit of RBDM
		 * @param enable nonzero param will enable, zero will disable
		 **/
		void setEnableRB( int enable );

		/**
		 * get Enable Bit of EBDM
		 * @return enable bit
		 **/
		unsigned int getEnableEB();

		/**
		 * get Enable Bit of RBDM
		 * @return enable bit
		 **/
		unsigned int getEnableRB();

		/**
		 * setDMAConfig set the DMA Controller operation mode
		 * @param config Bit mapping:
		 * TODO
		 **/
		void setDMAConfig( unsigned int config );

		/**
		 * getDMAConfig
		 * @return DMA Packetizer COnfiguration and Status
		 **/
		unsigned int getDMAConfig();

		/**
		 * set PCIe maximum payload size
		 * @param size maximum payload size in byte
		 **/
		void setMaxPayload( int size );

		/**
		 * set PCIe maximum payload size to the default
		 * value (MAX_PAYLOAD)
		 **/
		void setMaxPayload();

		/** INTERNAL ONLY, DO NOT CALL DIRECTLY!
		 * setMaxPayload( int size ) and setMaxPayload()
		 * are wrappers around _setMaxPayload and should
		 * be called instead
		 **/
		void _setMaxPayload( int size );

		/**
		 * get maximum payload size from current HW configuration
		 * @return maximum payload size in bytes
		 **/
		unsigned int getMaxPayload();

		/**
		 * get number of Scatter Gather entries for the Event buffer
		 * @return number of entries
		 **/
		unsigned int getEBDMnSGEntries();

		/**
		 * get number of Scatter Gather entries for the Report buffer
		 * @return number of entries
		 **/
		unsigned int getRBDMnSGEntries();

		/**
		 * get DMA Packetizer 'Busy' flag
		 * @return 1 if busy, 0 if idle
		 **/
		unsigned int getDMABusy();

		/**
		 * get buffer size set in EBDM. This returns the size of the 
		 * DMA buffer set in the DMA enginge and has to be the physical
		 * size of the associated DMA buffer.
		 * @return buffer size in bytes
		 **/
		unsigned long getEBSize();

		/**
		 * get buffer size set in RBDM. As the RB is not overmapped this size
		 * should be equal to the sysfs file size and buf->getRBSize()
		 * @return buffer size in bytes
		 **/
		unsigned long getRBSize();

		/**
		 * configure DMA engine for current set of buffers
		 * @param ebuf pointer to struct rorcfs_buffer to be used as 
		 * event buffer
		 * @param rbuf pointer to struct rorcfs_buffer to be used as 
		 * report buffer
		 * @param max_payload maximum payload size to be used (in bytes)
		 * @return 0 on sucess, <0 on error
		 * */
		int configureChannel(
				struct rorcfs_buffer *ebuf,
				struct rorcfs_buffer *rbuf,
				uint32_t max_payload);

		/**
		 * get base
		 * @return channel base address
		 **/
		unsigned int getBase() {
			return base;
		}

		/**
		 * get BAR
		 * @return bound rorcfs_bar
		 **/
		rorcfs_bar* getBar() {
			return bar;
		}

		/**
		 * set Event Buffer File Offset
		 * the DMA engine only writes to the Event buffer as long as
		 * its internal offset is at least (MaxPayload)-bytes smaller
		 * than the Event Buffer Offset. In order to start a transfer,
		 * set EBOffset to (BufferSize-MaxPayload). 
		 * IMPORTANT: offset has always to be a multiple of MaxPayload!
		 **/
		void setEBOffset( unsigned long offset );

		/**
		 * get current Event Buffer File Offset
		 * @return unsigned long offset
		 **/
		unsigned long getEBOffset();

		/**
		 * set Report Buffer File Offset
		 **/
		void setRBOffset( unsigned long offset );

		/**
		 * get Report Buffer File Offset
		 * @return unsigned long offset
		 **/
		unsigned long getRBOffset();

		/**
		 * setOffsets
		 * @param eboffset byte offset in event buffer
		 * @param rboffset byte offset in report buffer
		 **/
		void setOffsets( unsigned long eboffset, unsigned long rboffset );

		/**
		 * get buffer offset that is currently used as
		 * DMA destination
		 * @return 64bit offset in report buffer file
		 **/
		unsigned long getRBDMAOffset();

		/**
		 * get buffer offset that is currently used as
		 * DMA destination
		 * @return 64bit offset in event buffer file
		 **/
		unsigned long getEBDMAOffset();


		/**
		 * set DW in EBDM
		 * @param addr address in EBDM component
		 * @param data data to be writtem
		 **/
		void setEBDM( unsigned int addr, unsigned int data );

		/**
		 * get DW from EBDM
		 * @param addr address in EBDM component
		 * @return data read from EBDM
		 **/
		unsigned int getEBDM( unsigned int addr );

		/**
		 * set DW in RBDM
		 * @param addr address in RBDM component
		 * @param data data to be writtem
		 **/
		void setRBDM( unsigned int addr, unsigned int data );

		/**
		 * get DW from RBDM
		 * @param addr address in RBDM component
		 * @return data read from RBDM
		 **/
		unsigned int getRBDM( unsigned int addr );


		/**
		 * set DW in EBDRAM
		 * @param addr address in EBDRAM component
		 * @param data data to be writtem
		 **/
		void setEBDRAM( unsigned int addr, unsigned int data );

		/**
		 * memcpy into EBDRAM
		 * @param startaddr address in EBDRAM component
		 * @param dma_desc pointer to struct rorcfs_dma_desc
		 * */
		void memcpyEBDRAMentry(
				unsigned int startaddr, 
				struct rorcfs_dma_desc *dma_desc);

			/**
			 * get DW from EBDRAM
		 * @param addr address in EBDRAM component
		 * @return data read from EBDRAM
		 **/
		unsigned int getEBDRAM( unsigned int addr );

		/**
		 * set DW in RBDRAM
		 * @param addr address in RBDRAM component
		 * @param data data to be writtem
		 **/
		void setRBDRAM( unsigned int addr, unsigned int data );

		/**
		 * memcpy into RBDRAM
		 * @param startaddr address in EBDRAM component
		 * @param dma_desc pointer to struct rorcfs_dma_desc
		 * */
		void memcpyRBDRAMentry(
				unsigned int startaddr, 
				struct rorcfs_dma_desc *dma_desc);

		/**
		 * get DW from RBDRAM
		 * @param addr address in RBDRAM component
		 * @return data read from RBDRAM
		 **/
		unsigned int getRBDRAM( unsigned int addr );


		/**
		 * set DW in Packtizer
		 * @param addr address in PKT component
		 * @param data data to be writtem
		 **/
		void setPKT( unsigned int addr, unsigned int data );
  /**
   * set or unset single bit in register at address in PKT
   * @param addr aligned address within the packetizer regfile
   * @param pos bit possition to modify
   * @param enable true enables the bit
   **/
  void set_bitPKT(uint64_t addr, int pos, bool enable);

  /**
   * set or unset single bit in register at address in GTX
   * @param addr aligned address within the gtx regfile
   * @param pos bit possition to modify
   * @param enable true enables the bit
   **/
  void set_bitGTX(uint64_t addr, int pos, bool enable);

		/**
		 * get DW from  Packtizer
		 * @param addr address in PKT component
		 * @return data read from PKT
		 **/
		unsigned int getPKT( unsigned int addr );

		/**
		 * set DW in GTX Domain
		 * @param addr address in GTX component
		 * @param data data to be writtem
		 **/
		void setGTX( unsigned int addr, unsigned int data );

		/**
		 * get DW from GTX Domain
		 * @param addr address in GTX component
		 * @return data read from GTX
		 **/
		unsigned int getGTX( unsigned int addr );

		/**
		 * copy buffer range to Packetizer regfile
		 * @param addr address in PKT component
		 * @param source pointer to source data field
		 * @param num number of bytes to be copied to destination
		 * */
                 void set_memPKT( uint32_t addr, const void *source, size_t num); 
		/**
		 * copy buffer range from Packetizer regfile
		 * @param addr address in PKT component
		 * @param dest pointer to destination data field
		 * @param num number of bytes to be copied from destination
		 * */
                 void get_memPKT( uint32_t addr, void *dest, size_t num); 

		/**
		 * copy buffer range to GTX regfile
		 * @param addr address in GTX component
		 * @param source pointer to source data field
		 * @param num number of bytes to be copied to destination
		 * */
                 void set_memGTX( uint32_t addr, const void *source, size_t num);
		/**
		 * copy buffer range to GTX regfile
		 * @param addr address in GTX component
		 * @param dest pointer to destination data field
		 * @param num number of bytes to be copied from destination
		 * */
                 void get_memGTX( uint32_t addr, void *dest, size_t num);

	private:
		unsigned int base;
		rorcfs_bar *bar;
		unsigned int cMaxPayload;
};

#endif
