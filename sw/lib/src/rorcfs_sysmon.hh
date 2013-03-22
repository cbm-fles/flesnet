/**
 * @file rorcfs_sysmon.hh
 * @author Heiko Engel <hengel@cern.ch>
 * @version 0.1
 * @date 2011-11-16
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

#ifndef RORCFS_SYSMON_H
#define RORCFS_SYSMON_H

#include "rorc_registers.h"

/**
 * @class rorcfs_sysmon
 * @brief System monitor class
 *
 * This class can be attached to rorcfs_bar to provide access to the
 * static parts of the design, like PCIe status, SystemMonitor readings
 * and access to the ICAP interface
 **/
class rorcfs_sysmon
{
	public:
		rorcfs_sysmon();
		~rorcfs_sysmon();

		/**
		 * initialize instance
		 * @param bar parent rorcfs_bar instance
		 * @return -1 on errors, 0 on success
		 **/
		int init( rorcfs_bar *bar );

		/**
		 * get PCIe Interface status
		 * @return PCIe status consisting of:
		 * pcie_status_q[31:26] <= pl_ltssm_state;
		 * pcie_status_q[25:23] <= pl_initial_link_width;	
		 * pcie_status_q[22:20] <= pl_lane_reversal_mode;
		 * pcie_status_q[19] <= pl_link_gen2_capable;
		 * pcie_status_q[18] <= pl_link_partner_gen2_supported;
		 * pcie_status_q[17] <= pl_link_upcfg_capable;
		 * pcie_status_q[16:9] <= 7'b0;
		 * pcie_status_q[8] <= pl_sel_link_rate;
		 * pcie_status_q[7:2] <= 6'b0;
		 * pcie_status_q[1:0] <= pl_sel_link_width;
		 **/
		 unsigned int getPCIeStatus();

		 /**
			* get FPGA Firmware Revision
			* @return Firmware Revision
			**/
		 unsigned int getFwRevision();

		 /**
			* get FPGA Firmware Build Date
			* @return Firmware Build Date as combination of
			* year (bits [31:16]), month (bits[15:8]) and 
			* day (bits[7:0]).
			**/
		 unsigned int getFwBuildDate();

		 /**
			* get FPGA unique identifier (Device DNA)
			* @return 64bit Device DNA
			**/
		 unsigned long getDeviceDNA();

		 /**
			* get Fan Tach Value
			* @return RPMs of the FPGA Fan
			**/
		 unsigned int getFanTachValue();

		 /**
			* get FPGA Temperature
			* @return FPGA temperature in degree celsius
			**/
		 double getFPGATemperature();

		 /**
			* get FPGA VCC_INT Voltage
			* @return VCCINT in Volts
			**/
		 double getVCCINT();

		 /**
			* get FPGA VCC_AUX Voltage
			* @return VCCAUX in Volts
			**/
		 double getVCCAUX();

		 /**
			* write to ICAP Interface
			* @param dword bit-reordered configuration word
			*
			* use this function to write already reordered 
			* (*.bin-file) contents to the ICAP interface.
			**/
		 //void setIcapDin( unsigned int dword );

		 /**
			* write to ICAP Interface and do the bit reordering
			* @param dword not reordered configuration word
			*
			* use this function to write non-reordered 
			* (*.bit-files) to ICAP and do the reordering 
			* in the FPGA.
			**/
		 //void setIcapDinReorder( unsigned int dword );

		 /**
			* reset i2c bus
			* @return 0 on sucess, -1 on error
			**/
		 int i2c_reset();

		 /**
			* read byte from i2c memory location
			* @param slvaddr slave address
			* @param memaddr memory address
			* @param data pointer to unsigned char for 
			* received data
			* @return 0 on success, -1 on errors
			**/
		 int i2c_read_mem(
				 unsigned char slvaddr, 
				 unsigned char memaddr,
				 unsigned char *data);

		 /**
			* read byte from i2c memory location
			* @param slvaddr slave address
			* @param memaddr memory address
			* @param data pointer to unsigned char for 
			* received data
			* @return 0 on success, -1 on errors
			**/
		 int i2c_write_mem(
				 unsigned char slvaddr, 
				 unsigned char memaddr,
				 unsigned char data);

		 /**
			* i2c_set_config
			* @param config i2c configuration consisting of
			* prescaler (31:16) and ctrl (7:0)
			**/
		 void i2c_set_config(
				 unsigned int config);

	private:
		 rorcfs_bar *bar;

};

#endif
