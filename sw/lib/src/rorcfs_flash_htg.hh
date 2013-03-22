/**
 * @file rorcfs_flash_htg.hh
 * @author Heiko Engel <hengel@cern.ch>
 * @version 0.1
 * @date 2012-02-29
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
 * */

#ifndef _RORCLIB_RORCFS_FLASH_HTG
#define _RORCLIB_RORCFS_FLASH_HTG

/** Read array mode **/
#define FLASH_READ_ARRAY 0x00ff
/** Read identifier mode **/
#define FLASH_READ_IDENTIFIER 0x0090
/** Read status mode **/
#define FLASH_READ_STATUS 0x0070
/** Read CFI mode **/
#define FLASH_READ_CFI 0x0098

/** Timeout Value for waiting for SR7=1: 10 seconds **/
#define CFG_FLASH_TIMEOUT 0x0010000

/** bit mask for flash memory blocks **/
#define CFG_FLASH_BLKMASK 0xffff0000

/** Block erase setup **/
#define FLASH_CMD_BLOCK_ERASE_SETUP 0x0020
/** confirm **/
#define FLASH_CMD_CONFIRM 0x00d0
/** program/erase/suspend **/
#define FLASH_CMD_PROG_ERASE_SUSPEND 0x00b0
/** block lock setup **/
#define FLASH_CMD_BLOCK_LOCK_SETUP 0x0060
/** block lock confirm **/
#define FLASH_CMD_BLOCK_LOCK_CONFIRM 0x0001
/** buffer progam **/
#define FLASH_CMD_BUFFER_PROG 0x00e8
/** program setup **/
#define FLASH_CMD_PROG_SETUP 0x0040
/** configuration confirm **/
#define FLASH_CMD_CFG_REG_CONFIRM 0x0003
/** clear status **/
#define FLASH_CMD_CLR_STS 0x0050
/** BC setup **/
#define FLASH_CMD_BC_SETUP 0x00bc
/** BC setup **/
#define FLASH_CMD_BC_CONFIRM 0x00cb
/** flash busy flag mask **/
#define FLASH_PEC_BUSY 1<<7



/**
 * @class rorcfs_flash_htg
 * @brief interface class to the StrataFlash Embedded 
 * Memory P30-65nm on the HTG board
 **/
class rorcfs_flash_htg
{
	public:

	/**
	 * constructor
	 * @param bar rorcfs_bar instance representing the flash
	 * memory
	 **/
	rorcfs_flash_htg(rorcfs_bar *bar);

	/**
	 * deconstructor
	 **/
	~rorcfs_flash_htg();

	/**
	 * set read state
	 * @param state one of FLASH_READ_ARRAY, FLASH_READ_IDENTIFIER,
	 * FLASH_READ_STATUS, FLASH_READ_CFI
	 * @param addr address
	 **/
	void setReadState(unsigned short state, unsigned int addr);

	/**
	 * read flash status register
	 * @param blkaddr block address
	 **/ 
	unsigned short getStatusRegister(unsigned int blkaddr);

	/**
	 * read flash status register
	 * @param blkaddr block address
	 **/ 
	void clearStatusRegister(unsigned int blkaddr);

	/**
	 * get Manufacturer Code
	 * @return manufacturer code
	 **/
	unsigned short getManufacturerCode();

	/**
	 * get Device ID
	 * @return Device ID
	 **/
	unsigned short getDeviceID();

	/**
	 * get Block Lock Configuration
	 * @param blkaddr block address
	 * @return block lock configuration: 0=unlocked, 
	 * 1=locked but not locked down, 3=locked and locked down
	 **/
	unsigned short getBlockLockConfiguration(unsigned int blkaddr);

	/**
	 * get Read Configuration Register (RCR)
	 * @return Read Configuraion
	 **/
	unsigned short getReadConfigurationRegister();

	/**
	 * Program single Word to destination address
	 * @param addr address to be written to
	 * @param data data to be written
	 * @return 0 on success, -1 on errors
	 **/
	int programWord(unsigned int addr, unsigned short data);

	/** get WORD from flash
	 * @param addr address
	 * @return data word at specified address
	 **/
	unsigned short get(unsigned int addr);


	/**
	 * Buffer Program Mode
	 * @param addr start address
	 * @param length number of WORDs to be written
	 * @param data pointer to data buffer
	 * @return 0 on sucess, -1 on errors
	 **/
	int programBuffer(
			unsigned int addr, 
			unsigned short length, 
			unsigned short *data);

	/**
	 * Erase Block
	 * @param blkaddr block address
	 * @return 0 on sucess, -1 on errors
	 **/
	int eraseBlock(unsigned int blkaddr);


	/**
	 * programSuspend
	 * @param blkaddr block address
	 **/
	void programSuspend(unsigned int blkaddr);


	/**
	 * programResume
	 * @param blkaddr block address
	 **/
	void programResume(unsigned int blkaddr);


	/**
	 * Lock Block 
	 **/
	void lockBlock(unsigned int blkaddr);


	/**
	 * unlock Block 
	 **/
	void unlockBlock(unsigned int blkaddr);

	/**
	 * set Configuration Register
	 * */
	void setConfigReg(unsigned short value);

	/**
	 * check if block is empty
	 * NOTE: this will only work if VPP=VPPH (~8V)
	 * this is not supported on C-RORC
	 * @param blkaddr block address
	 * @return -1 on error, 0 on empty, 1 on not empty
	 * */
	int blankCheck(unsigned int blkaddr);


	/**
	 * get Block Address
	 * @param addr address
	 * @return associated block address
	 * */
	unsigned int getBlockAddress( unsigned int addr );


	/**
	 * get Bank Address
	 * @param addr address
	 * @return associated bank address
	 * */
	unsigned int getBankAddress( unsigned int addr );

	private:
	rorcfs_bar *bar;
	unsigned short read_state;
};


#endif
