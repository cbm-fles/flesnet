/**
 * @file rorcfs_pattern_generator.hh
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

#ifndef _RORCLIB_RORCFS_PATTERN_GENERATOR_H
#define _RORCLIB_RORCFS_PATTERN_GENERATOR_H

/** enable pattern generator */
#define PG_ENABLE 0x00000001 
/** disable pattern generator */
#define PG_DISABLE 0x00000000 

/** run pattern generator in continuous mode */
#define PG_CONTINUOUS (0x00000001<<2)
/** enable Flow Control. This respects XON/XOFF signals
 * before providing PG data. Always enable this unless
 * you know what you are doing!!*/
#define PG_FLOW_CONTROL (0x00000001<<1)
/** increment pattern with each DWord */
#define PG_INC (0x00000000<<4)
/** decrement pattern with each DWord */
#define PG_DEC (0x00000002<<4)
/** rotate pattern left with each DWord */
#define PG_SHIFT (0x00000001<<4)
/** invert pattern bitwise with each DWord */
#define PG_TOGGLE (0x00000003<<4)

/**
 * @class rorcfs_pattern_generator
 * @brief pattern generator class
 *
 * pattern generator can be attached to an existing
 * DIU instance (rorcfs_diu) and can produces
 * dummy data.
 **/
class rorcfs_pattern_generator
{
	public:
		rorcfs_pattern_generator();
		~rorcfs_pattern_generator();

		/**
		 * initialize patter generator with
		 * underlying DMA channel
		 * @return 0 on sucess, -1 on errors
		 **/
		int init(rorcfs_diu *diu);

		/**
		 * Set Pattern Generator Mode
		 *
		 * \li PG_ENABLE: enable the pattern generator
		 * \li PG_CONTINUOUS: continue operation as long as enabled. The alternative
		 * is to use setNumEvents() to specifiy a number of events to be sent
		 * \li PG_FLOW_CONTROL: enable Flow Control. This respects XON/XOFF signals
		 * before providing PG data. Always enable this unless you know what 
		 * you are doing!!
		 * \li PG_INC: increment pattern with each DWord
		 * \li PG_DEC: decrement pattern with each DWord
		 * \li PG_SHIFT: rotate pattern left with each DWord
		 * \li PG_TOGGLE: invert pattern bitwise with each DWord
		 *
		 * \note use only one of PG_INC/PG_DEC/PG_SHIFT/PG_TOGGLE
		 *
		 * @param mode use bitwise or of PG_ENABLE, PG_CONTINUOUS,
		 * PG_FLOW_CONTROL, PG_INC, PG_DEC, PG_SHIFT, PG_TOGGLE
		 **/
		void setMode( unsigned int mode );

		/**
		 * Get pattern generator status word
		 * @return unsigned int status
		 **/
		unsigned int getMode();

		/**
		 * Set wait time between events
		 * @param wait_time in clock cycles
		 **/
		void setWaitTime( unsigned int wait_time );

		/**
		 * get wait time between events
		 * @return unsigned int wait time in clock cycles
		 **/
		unsigned int getWaitTime();

		/**
		 * Set number of events for non-continuous mode
		 * @param num number of events
		 **/
		void setNumEvents( unsigned int num );

		/**
		 * Get number of events
		 * @return number of events to be sent
		 **/
		unsigned int getNumEvents();

		/**
		 * set length of a single event
		 * @param length number of payload DWs
		 **/
		void setEventLength( unsigned int length );

		/**
		 * get length of a single event
		 * @return unsigned int payload length in DWs
		 **/
		unsigned int getEventLength();

		/**
		 * set starting pattern for selected pattern mode
		 * @param pattern usigned int pattern
		 **/
		void setPattern( unsigned int pattern );

		/**
		 * get pattern
		 * @return unsigned int pattern
		 **/
		unsigned int getPattern();

	private:
		rorcfs_bar *bar;
		unsigned int base;
};

#endif
