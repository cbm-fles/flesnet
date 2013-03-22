//==============================================================================
// FLI CRORC Interface
//
// (previously: FLI TX/RX UART Interface
// Set up socket, transmit/receive string to/from UART. )
//
// This library is free software; you can redistribute it and/or modify it 
// under the terms of the GNU Lesser General Public License as published 
// by the Free Software Foundation; either version 2.1 of the License, or 
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.   See the GNU Lesser General Public 
// License for more details.   See http://www.gnu.org/copyleft/lesser.txt
//
//------------------------------------------------------------------------------
// Version   Author          Date          Changes
// 0.1       Hans Tiggeler   03 Aug 2003   Tested on Modelsim SE 5.7e
// 0.2       Heiko Engel     22 Sep 2011   Adjusted for CRORC, Modelsim SE 10.0c_1
//==============================================================================

#ifdef SIM

#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include "mti.h"                    // MTI Headers & Prototypes

//========================== Sockets Definitions ======================
#include <unistd.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>              // defines gethostbyname()

typedef int SOCKET;
#define FMT_MASK  0x7F00000000000000
#define FMT_MWR64 0x6000000000000000
#define FMT_MWR32 0x4000000000000000

#define ADDR_OFFSET 0xfa000000
#define MAX_PAYLOAD_BYTES 128

#ifdef FLI_DEBUG
#define fli_debug(fmt, args...) mti_PrintFormatted(fmt, ## args)
#else
#define fli_debug(fmt, args...)
#endif

typedef enum {
	STD_LOGIC_U,
	STD_LOGIC_X,
	STD_LOGIC_0,
	STD_LOGIC_1,
	STD_LOGIC_Z,
	STD_LOGIC_W,
	STD_LOGIC_L,
	STD_LOGIC_H,
	STD_LOGIC_D
} StdLogicType;

// signals to be monitored/controllen in pcie_io.vhd
typedef struct {
	mtiSignalIdT clk ;                        
	mtiDriverIdT wr_en;
	mtiSignalIdT wr_done;
	mtiDriverIdT *addr;
	mtiDriverIdT *wr_len;
	mtiDriverIdT *pWrData[MAX_PAYLOAD_BYTES>>2];
	mtiSignalIdT pWrDataId;
	mtiDriverIdT *bar;
	mtiDriverIdT *byte_enable;
	mtiDriverIdT *tag;
	mtiSignalIdT rd_data;
	mtiSignalIdT rd_done;
	mtiDriverIdT rd_en;

	mtiSignalIdT td;
	mtiSignalIdT teof_n;
	mtiSignalIdT tsof_n;
	mtiSignalIdT trem_n;
	mtiSignalIdT tsrc_rdy_n;
	mtiSignalIdT tdst_rdy_n;

	mtiDriverIdT *id;

	SOCKET       sdsc;
	SOCKET       sdsc_acc;
	SOCKET       sddma;
	SOCKET       sddma_acc;
} inst_rec;

// FLI-Message returned to client request
typedef struct {
	int wr_ack;
	unsigned int data;
	int id;
} flimsgT;

// FLI-Command from client
typedef struct {
	unsigned int addr;
	unsigned int bar;
	unsigned char byte_enable;
	unsigned char type;
	unsigned short len;
	int id;
} flicmdT;

/** bit encodings for command type **/
#define CMD_READ (1<<0)
#define CMD_WRITE (1<<1)
#define CMD_TIME (1<<2)


int msgid;

// DMA Monitor globals
int rx_frame64, rx_frame32=0, rx_dvld=0;
int rx_ndw = 0;
unsigned long rx_data = 0;
unsigned long rx_addr = 0;
unsigned char *buffer, *buffer_ptr;
int buffer_size;


// Prototypes
static void tx_fli(void *param);
static void rx_fli(void *param);
static void dma_monitor( void *param );
static void sockreader( void *param );
int init_sockets(char *hostname, int port, int *sd, int *sdack);
void close_socket(int *sd);
void set_bar( mtiDriverIdT *drivers, unsigned int value );
void set_slv8( mtiDriverIdT *drivers, unsigned char value );
void set_slv32( mtiDriverIdT *drivers, unsigned int value );
void set_slv( mtiDriverIdT *drivers, unsigned int data, int bit_width);

mtiUInt32T conv_std_logic_vector(mtiSignalIdT stdvec);
unsigned long slv2ulong(mtiSignalIdT stdvec);
unsigned long slv2ulong_high(mtiSignalIdT stdvec);
unsigned long slv2ulong_low(mtiSignalIdT stdvec);
//void sockCB(void * param);          // Receive socket callback
void loadDoneCB(void * param);       // Add socket callback
void restartCallback(void *param);  // User types quit/restart 


/**
 * FLI code entry point
 * this function is called from FOREIGN attribute in fli.vhd
 **/
void fli_init( 
		mtiRegionIdT region, 
		char *param, 
		mtiInterfaceListT *generics, 
		mtiInterfaceListT *ports)
{
	inst_rec    *ip;                                   // Declare ports            
	mtiProcessIdT rxproc, txproc, dmaproc, sockreaderproc; // current process id
	char        hostname[] = "localhost";
	int         port=2000;
	int i;

	// allocate memory for ports
	ip = (inst_rec *)mti_Malloc(sizeof(inst_rec));  

	// bind Signals/Drivers to VHDL signals
	ip->clk     = mti_FindSignal( "system_tb/FLI_SIM/CIO/clk");
	ip->wr_en   = mti_CreateDriver( 
			mti_FindSignal( "system_tb/FLI_SIM/CIO/wr_en") );
	ip->wr_done = mti_FindSignal( "system_tb/FLI_SIM/CIO/wr_done");
	ip->addr     = mti_GetDriverSubelements(
			mti_CreateDriver(	
				mti_FindSignal( "system_tb/FLI_SIM/CIO/addr") ), 0);
	ip->bar     = mti_GetDriverSubelements(
			mti_CreateDriver(	mti_FindSignal( "system_tb/FLI_SIM/CIO/bar") ), 0);	
	ip->byte_enable = mti_GetDriverSubelements(
			mti_CreateDriver(	
				mti_FindSignal( "system_tb/FLI_SIM/CIO/byte_enable") ), 0);
	ip->tag = mti_GetDriverSubelements(
			mti_CreateDriver(	
				mti_FindSignal( "system_tb/FLI_SIM/CIO/tag") ), 0);
		ip->wr_len = mti_GetDriverSubelements(
			mti_CreateDriver(	
				mti_FindSignal( "system_tb/FLI_SIM/CIO/wr_len") ), 0);

	// drive pWrData array (0 to 31) of std_logic_vector(31 downto 0)
	ip->pWrDataId = mti_FindSignal("system_tb/FLI_SIM/CIO/pWrData");
	int ticklen = mti_TickLength(	mti_GetSignalType(ip->pWrDataId));
	mtiSignalIdT *elem_list = mti_GetSignalSubelements(ip->pWrDataId, 0);
	if(ticklen < (MAX_PAYLOAD_BYTES>>2)) {
		printf("FLI_WARNING: flisock MAX_PAYLOAD_BYTES is larger than VHDL"
				"array size\n");
	}
	for(i=0; i<ticklen; i++) {
		ip->pWrData[i] = mti_GetDriverSubelements(
				mti_CreateDriver(
					elem_list[i]), 0);
	}


	ip->rd_data = mti_FindSignal( "system_tb/FLI_SIM/CIO/rd_data");
	ip->rd_done = mti_FindSignal( "system_tb/FLI_SIM/CIO/rd_done");
	ip->rd_en   = mti_CreateDriver( 
			mti_FindSignal( "system_tb/FLI_SIM/CIO/rd_en") );

	ip->id      = mti_GetDriverSubelements(
			mti_CreateDriver( 
				mti_FindSignal( "system_tb/FLI_SIM/CIO/id") ), 0);

	// DMA monitoring
	ip->td         = mti_FindSignal( "system_tb/FLI_SIM/CIO/trn_td" );
	ip->tsrc_rdy_n = mti_FindSignal( "system_tb/FLI_SIM/CIO/trn_tsrc_rdy_n" );
	ip->tdst_rdy_n = mti_FindSignal( "system_tb/FLI_SIM/CIO/trn_tdst_rdy_n" );
	ip->trem_n     = mti_FindSignal( "system_tb/FLI_SIM/CIO/trn_trem_n" );
	ip->tsof_n     = mti_FindSignal( "system_tb/FLI_SIM/CIO/trn_tsof_n" );
	ip->teof_n     = mti_FindSignal( "system_tb/FLI_SIM/CIO/trn_teof_n" );

	rxproc = mti_CreateProcess("rx_fli", rx_fli, ip);
	mti_Sensitize(rxproc, ip->clk, MTI_EVENT);

	txproc = mti_CreateProcess("tx_fli", tx_fli, ip);
	mti_Sensitize(txproc, ip->clk, MTI_EVENT);

	dmaproc = mti_CreateProcess("dma_monitor", dma_monitor, ip);
	mti_Sensitize(dmaproc, ip->clk, MTI_EVENT);

	mti_PrintFormatted("Opening socket %d on %s\nWaiting for connection.....\n",
			port+1,hostname);

	// Get socket descriptor
	if ((init_sockets(hostname, port+1, &ip->sddma, &ip->sddma_acc))==-1) {   
		mti_PrintMessage("*** Socket init error ***\n");
		mti_FatalError(); 
	}                              

	mti_PrintFormatted("Opening socket %d on %s\nWaiting for connection.....\n",
			port,hostname);

	// Get socket descriptor
	if ((init_sockets(hostname, port, &ip->sdsc, &ip->sdsc_acc))==-1) {   
		mti_PrintMessage("*** Socket init error ***\n");
		mti_FatalError(); 
	}     

	sockreaderproc = mti_CreateProcess("sockreader", sockreader, ip);
	mti_Sensitize(sockreaderproc, ip->clk, MTI_EVENT); 


	mti_AddQuitCB(restartCallback, ip);
	mti_AddRestartCB(restartCallback, ip); // Add quit or restart CallBack

	mti_AddLoadDoneCB((mtiVoidFuncPtrT)loadDoneCB, ip);
}


void restartCallback(void *param)  // User types quit/restart 
{

	inst_rec *ip = (inst_rec *)param;
	rx_frame64 = 0;
	rx_frame32 = 0;
	rx_dvld = 0;
	if(buffer) {
		mti_Free(buffer);
		buffer = NULL;
		buffer_ptr = NULL;
	}
	close_socket(&ip->sdsc);
	close_socket(&ip->sdsc_acc);
	close_socket(&ip->sddma);
	close_socket(&ip->sddma_acc);
	mti_Free(param);
}


/**
 * RX_FLI: watch rd_done flag and send data to client, release rd_en flag
 **/
static void rx_fli(void *param)
{	
	inst_rec * ip = (inst_rec *)param;
	flimsgT msg;

	if (mti_GetSignalValue(ip->clk)==STD_LOGIC_1) {  // rising_edge(clk)
		if (mti_GetSignalValue(ip->rd_done)==STD_LOGIC_1) { // read done?
			msg.wr_ack = 0;
			msg.data = conv_std_logic_vector(ip->rd_data);
			msg.id = msgid;
			mti_ScheduleDriver( ip->rd_en, STD_LOGIC_0, 0, MTI_INERTIAL );
			//mti_PrintFormatted("%d: rd_done==1\n", mti_Now());
			send(ip->sdsc_acc, &msg, sizeof(msg), 0);
			fli_debug("%d FLI CMD %d served: %x\n", 
					mti_Now(), msg.id, msg.data);
		}
	}
}

/**
 * TX_FLI: watch wr_done flag and send ACK to client, release wr_en
 **/
static void tx_fli(void *param)
{
	inst_rec * ip = (inst_rec *)param;
	flimsgT msg;

	if (mti_GetSignalValue(ip->clk)==STD_LOGIC_1) {  // rising_edge(clk)
		if (mti_GetSignalValue(ip->wr_done)==STD_LOGIC_1) { // write done?
			msg.wr_ack = 1;
			msg.data = 0;
			msg.id = msgid;
			mti_ScheduleDriver( ip->wr_en, STD_LOGIC_0, 0, MTI_INERTIAL );
			//mti_PrintFormatted("%d: wr_done==1\n", mti_Now());
			send(ip->sdsc_acc, &msg, sizeof(msg), 0);
			fli_debug("%d FLI CMD %d served\n", mti_Now(), msg.id);
		}
	}
}

/**
 * change byte ordering of 64bit trn_td QWord
 * {[7:0],[15:8],[23:16],[31:24],[39:32],[47:40],[55:48],[63:56]}
 **/
unsigned long byte_reorder( unsigned long qw )
{
	unsigned long ret=0;
	int i;
	for(i=0; i<8; i++)
		ret += (((unsigned long)(qw>>(8*i)) & 0x00000000000000ff) << (56-8*i));
	return ret;
}

/**
 * change byte ordering of 32bit trn_td DWord
 * {[7:0],[15:8],[23:16],[31:24]}
 **/
unsigned int byte_reorder32( unsigned int dw )
{
	unsigned int ret=0;
	int i;
	for(i=0; i<4; i++)
		ret += (((unsigned int)(dw>>(4*i)) & 0x000000ff) << (28-4*i));
	return ret;
}


static void sockreader( void *param )
{
	inst_rec *ip = (inst_rec *)param;
	SOCKET sock = ip->sdsc_acc;
	int i;
	flicmdT cmd;
	flimsgT msg;
	unsigned int *buffer;
	unsigned int  buffersize;

	if (mti_GetSignalValue(ip->clk)==STD_LOGIC_1) { //rising_edge(clk)

		inst_rec * ip = (inst_rec *)param;
		i = read(sock, &cmd, sizeof(cmd));
		if (i == 0)  {   // terminate if 0 characters received
			mti_PrintMessage("closing socket\n");
			close((SOCKET)sock );
			mti_Break();
		} else if (i!=-1) {	

			// address and bar have to be set on each kind of request
			set_slv32( ip->addr, cmd.addr + ADDR_OFFSET );
			set_slv8( ip->byte_enable, cmd.byte_enable );
			set_slv8( ip->tag, cmd.id );
			set_bar( ip->bar, cmd.bar );
			set_slv32( ip->id, cmd.id );

			switch (cmd.type) {
				case CMD_READ:
					// is a read command, no data following
					fli_debug("%d ** FLI_READ bar=%d, addr=%x, be=%x, id=%d\n",
							mti_Now(), cmd.bar, cmd.addr, 
							cmd.byte_enable, cmd.id);
					fflush(stdout);

					// read request
					mti_ScheduleDriver(ip->rd_en, STD_LOGIC_1, 0, MTI_INERTIAL);
					break;

				case CMD_WRITE:
					// is a write command, read following DWs
					buffersize = cmd.len*sizeof(unsigned int);
					buffer = (unsigned int *)mti_Malloc(buffersize);
					i = read(sock, buffer, buffersize);
					if (i == 0)  {   // terminate if 0 characters received
						mti_PrintMessage("failed to read command payload, "
								"closing socket\n");
						close((SOCKET)sock );
						mti_Break();
					} else if (i!=-1) {	
						// payload received
						fli_debug("%d ** FLI WRITE: bar=%d, "
								"addr=%x, len=%d, byte_enable=%x, id=%d, "
								"data0=%08x\n",
								mti_Now(), cmd.bar, cmd.addr, 
								cmd.len, cmd.byte_enable, cmd.id, buffer[0]);
						fflush(stdout);

						mti_ScheduleDriver(ip->wr_en, STD_LOGIC_1, 0, MTI_INERTIAL);
						set_slv( ip->wr_len, (unsigned int)cmd.len, 10);
						for(i=0; i<cmd.len; i++) {
							set_slv(ip->pWrData[i], buffer[i], 32);
						}
					}
					mti_Free(buffer);
					break;

				case CMD_TIME:
					msg.data = mti_Now();
					msg.wr_ack = 0;
					msg.id = cmd.id;
					send(ip->sdsc_acc, &msg, sizeof(msg), 0);
					//fli_debug("%d *** FLI TIME ***\n", msg.data);
					break;
			}
			msgid = cmd.id;
		}
	}
}




static void dma_monitor( void *param )
{
	inst_rec *ip = (inst_rec *)param;
	unsigned long tmp64, rx_trem;
	unsigned int tmp32;
	unsigned char ack = 0;

	if (mti_GetSignalValue(ip->clk)==STD_LOGIC_1) { //rising_edge(clk)
		if (mti_GetSignalValue(ip->tsrc_rdy_n)==STD_LOGIC_0 &&
				mti_GetSignalValue(ip->tdst_rdy_n)==STD_LOGIC_0) {

			// valid data
			if (rx_dvld) {

				rx_data = byte_reorder(slv2ulong_high(ip->td));
				rx_trem = slv2ulong(ip->trem_n);

				// data on [127:?], but not [127:0]
				if (rx_trem != 0  && mti_GetSignalValue(ip->teof_n)==STD_LOGIC_0) { 
					switch (rx_trem) {
						case 1:
							rx_ndw -= 3;
							tmp32 = (unsigned int)(rx_data & 0xffffffff);
							memcpy( buffer_ptr, &tmp32, sizeof(unsigned int) );
							buffer_ptr += sizeof(unsigned int);
							//mti_PrintFormatted("***memcpy32 %08x to ptr %p\n", tmp32, buffer_ptr);	
							
							tmp32 = (unsigned int)(rx_data >> 32);
							memcpy( buffer_ptr, &tmp32, sizeof(unsigned int) );
							buffer_ptr += sizeof(unsigned int);
							//mti_PrintFormatted("***memcpy32 %08x to ptr %p\n", tmp32, buffer_ptr);

							rx_data = byte_reorder(slv2ulong_low(ip->td));

							tmp32 = (unsigned int)(rx_data & 0xffffffff);
							memcpy( buffer_ptr, &tmp32, sizeof(unsigned int) );
							buffer_ptr += sizeof(unsigned int);
							//mti_PrintFormatted("***memcpy32 %08x to ptr %p\n", tmp32, buffer_ptr);
							break;

						case 2:
							rx_ndw -= 2;
							tmp32 = (unsigned int)(rx_data & 0xffffffff);
							memcpy( buffer_ptr, &tmp32, sizeof(unsigned int) );
							buffer_ptr += sizeof(unsigned int);
							//mti_PrintFormatted("***memcpy32 %08x to ptr %p\n", tmp32, buffer_ptr);	
							
							tmp32 = (unsigned int)(rx_data >> 32);
							memcpy( buffer_ptr, &tmp32, sizeof(unsigned int) );
							buffer_ptr += sizeof(unsigned int);
							//mti_PrintFormatted("***memcpy32 %08x to ptr %p\n", tmp32, buffer_ptr);
							break;

						case 3:
							rx_ndw -= 1;
							tmp32 = (unsigned int)(rx_data & 0xffffffff);
							memcpy( buffer_ptr, &tmp32, sizeof(unsigned int) );
							buffer_ptr += sizeof(unsigned int);
							//mti_PrintFormatted("***memcpy32 %08x to ptr %p\n", tmp32, buffer_ptr);
							break;

						default:
							break;
					} //switch(rx_trem)

				} else { //data on td[127:0]
					rx_ndw -= 4;

					tmp32 = (unsigned int)(rx_data & 0xffffffff);
					memcpy( buffer_ptr, &tmp32, sizeof(unsigned int) );
					buffer_ptr += sizeof(unsigned int);
					//mti_PrintFormatted("***memcpy32 %08x to ptr %p\n", tmp32, buffer_ptr);
				
					tmp32 = (unsigned int)(rx_data >> 32);
					memcpy( buffer_ptr, &tmp32, sizeof(unsigned int) );
					buffer_ptr += sizeof(unsigned int);
					//mti_PrintFormatted("***memcpy32 %08x to ptr %p\n", tmp32, buffer_ptr);
				
					rx_data = byte_reorder(slv2ulong_low(ip->td));

					tmp32 = (unsigned int)(rx_data & 0xffffffff);
					memcpy( buffer_ptr, &tmp32, sizeof(unsigned int) );
					buffer_ptr += sizeof(unsigned int);
					//mti_PrintFormatted("***memcpy32 %08x to ptr %p\n", tmp32, buffer_ptr);
				
					tmp32 = (unsigned int)(rx_data >> 32);
					memcpy( buffer_ptr, &tmp32, sizeof(unsigned int) );
					buffer_ptr += sizeof(unsigned int);
					//mti_PrintFormatted("***memcpy32 %08x to ptr %p\n", tmp32, buffer_ptr);

					//memcpy( buffer_ptr, &rx_data, sizeof(rx_data) );
					//buffer_ptr += sizeof(rx_data);
					//mti_PrintFormatted("***memcpy64 %016lx to ptr %p\n", rx_data, buffer_ptr);
				}

				// check for protocol error
				if ( (rx_ndw==0 && mti_GetSignalValue(ip->teof_n)!=STD_LOGIC_0 ) ||
						 (rx_ndw!=0 && mti_GetSignalValue(ip->teof_n)==STD_LOGIC_0 ) ||
						 rx_ndw < 0 ) {
					mti_PrintFormatted("%d Protocol Error: nDW in HDR does "
							"not comply with #DWs sent (%d)\n", mti_Now(), rx_ndw);
					rx_dvld = 0;
					mti_Free(buffer);
					buffer = NULL;
					buffer_ptr = NULL;

				} else if (mti_GetSignalValue(ip->teof_n)==STD_LOGIC_0) {
					// send data
					send(ip->sddma_acc, buffer, buffer_size, 0);
					while( read( ip->sddma_acc, &ack, 1 ) != 1 );
					ack = 0;
					rx_dvld = 0;
					rx_ndw = 0;
					mti_Free(buffer);
					buffer = NULL;
					buffer_ptr = NULL;
					buffer_size = 0;
				}
			}

			// wait for SOF with MWR64 or MWR32 request
			if (mti_GetSignalValue(ip->tsof_n)==STD_LOGIC_0 && rx_dvld==0 &&
					( ((slv2ulong_high(ip->td) & FMT_MASK) == FMT_MWR64) || 
						((slv2ulong_high(ip->td) & FMT_MASK) == FMT_MWR32)) ) 
			{
				// number of DWs: td[41:32]
				rx_ndw = ( slv2ulong_high(ip->td)>>32 ) & 0x3ff;

				//allocate buffer for DWs
				buffer_size = rx_ndw*sizeof(unsigned int) + sizeof(unsigned long);
				buffer = (unsigned char *)mti_Malloc( buffer_size );
				buffer_ptr = buffer;
				if (!buffer)
					perror("dma_monitor: malloc buffer");

				if ((slv2ulong_high(ip->td) & FMT_MASK) == FMT_MWR64) {
					rx_addr = slv2ulong_low(ip->td);
					memcpy(buffer, &rx_addr, sizeof(rx_addr));
					buffer_ptr += sizeof(rx_addr);
					fli_debug("** FLI DMA MON: MWR64 TSOF, %d DWs to %lx\n", 
							rx_ndw, rx_addr);

				} else {

					rx_data = slv2ulong_low(ip->td);
					tmp64 = (unsigned long)(rx_data>>32);
					memcpy(buffer, &tmp64, sizeof(unsigned long));
					buffer_ptr += sizeof(unsigned long);
					fli_debug("** FLI DMA MON: MWR32 TSOF, %d DWs to %x\n", 
							rx_ndw, tmp64);

					tmp32 = (unsigned int)(byte_reorder(rx_data) >> 32);
					memcpy( buffer_ptr, &tmp32, sizeof(unsigned int) );
					buffer_ptr += sizeof(unsigned int);
					//mti_PrintFormatted("***memcpy32 %08x to ptr %p\n", tmp32, buffer_ptr);
					rx_ndw -= 1;
				}

				rx_dvld = 1;
			}

		} // tsrc_rdy_n=='0'

	} // rising_edge

} //process

/**
 * initialize sockets, start server
 **/
int init_sockets(char *hostname, int port, int *sd, int *sdack)
{
	int     statusFlags;
	unsigned int     addrlen;
	struct  sockaddr_in sin;
	struct  sockaddr_in pin;
	struct  hostent *hp;

	// Request Stream socket from OS using default protocol
	if ((*sd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ) {
		mti_PrintMessage("*** Error opening socket ***\n" );
		return -1;
	}

	// get host machine info
	if ((hp = gethostbyname(hostname)) == 0) {
		mti_PrintFormatted( "%s: Unknown host.\n", hostname );
		close_socket(sd);
		return -1;
	}

	// fill in the socket structure with host information 
	memset(&sin, 0, sizeof(sin));               // Clear Structure
	sin.sin_family = AF_INET;                   // Specify Address format
	sin.sin_addr.s_addr = INADDR_ANY;//((struct in_addr *)(hp->h_addr))->s_addr;
	sin.sin_port = htons((unsigned short)port); // set port number

	// bind the socket to the port number 
	if (bind(*sd, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
		mti_PrintMessage("*** Error: Bind\n");
		close_socket(sd);
		return -1;
	}

	// Listen
	if (listen(*sd, 5) == -1) {
		mti_PrintMessage("*** Error: Listen\n");
		close_socket(sd);
		return -1;
	}

	// Accept connection from client, create new socket for communication
	addrlen = sizeof(struct sockaddr_in);
	//if ((*sd_acc = accept(*sd, (struct sockaddr *)  &pin, &addrlen)) == -1) {
	if ((*sdack = accept(*sd, (struct sockaddr *)  &pin, &addrlen)) == -1) {
		mti_PrintMessage("*** Error: Accept\n");
		close_socket(sd);
		return -1;
	}

/*	statusFlags = fcntl(ip->sdsc, F_GETFL );
	if ( statusFlags == -1 ) {
		perror( "Getting socket status" );
	} else {
		int ctlValue;
		statusFlags |= O_NONBLOCK;
		ctlValue = fcntl( ip->sdsc, F_SETFL, statusFlags );
		if (ctlValue == -1 ) {
			perror("Setting listen socket status");
		}
	} */

	statusFlags = fcntl( *sdack, F_GETFL );
	if ( statusFlags == -1 ) {
		perror("Getting listen socket status");
	} else {
		int ctlValue;
		statusFlags |= O_NONBLOCK;
		ctlValue = fcntl( *sdack, F_SETFL, statusFlags );
		if (ctlValue == -1 ) {
			perror("Setting socket status");
		}
	}

	mti_PrintFormatted("Server: connect from host\n");
	return 0;
}


/**
 * Close Socket
 **/
void close_socket(int *sd)
{
	close(*sd);
	mti_PrintMessage("**** Socket removed ****\n");
}


/**
 * Convert std_logic_vector into an integer
 **/
mtiUInt32T conv_std_logic_vector(mtiSignalIdT stdvec)
{
	mtiSignalIdT *  elem_list;
	mtiTypeIdT      sigtype;
	mtiInt32T       i,num_elems;
	mtiUInt32T      retvalue,shift; 

	sigtype = mti_GetSignalType(stdvec);  // signal type
	num_elems = mti_TickLength(sigtype);  // Get number of elements

	elem_list = mti_GetSignalSubelements(stdvec, 0);

	shift=(mtiUInt32T) pow(2.0,(double)num_elems-1);// start position

	retvalue=0;
	for (i=0; i < num_elems; i++ ) {
		if (mti_GetSignalValue(elem_list[i])==3) {
			retvalue=retvalue+shift;
		} 
		shift=shift>>1;
	}

	mti_VsimFree(elem_list);

	return(retvalue);
}

/**
 * Convert std_logic_vector into an unsigned long
 **/
unsigned long slv2ulong(mtiSignalIdT stdvec)
{
	mtiSignalIdT *  elem_list;
	mtiTypeIdT      sigtype;
	unsigned long   i,num_elems;
	unsigned long   retvalue,shift; 

	sigtype = mti_GetSignalType(stdvec);  // signal type
	num_elems = mti_TickLength(sigtype);  // Get number of elements

	elem_list = mti_GetSignalSubelements(stdvec, 0);

	shift=(unsigned long) pow(2.0,(double)num_elems-1);// start position

	retvalue=0;
	for (i=0; i < num_elems; i++ ) {
		if (mti_GetSignalValue(elem_list[i])==3) {
			retvalue=retvalue+shift;
		} 
		shift=shift>>1;
	}

	mti_VsimFree(elem_list);

	//mti_PrintFormatted("slv2ulong: num_elems=%d, sizeof(ret)=%d, ret=%llx\n",
	//		num_elems, sizeof(retvalue), retvalue);

	return(retvalue);
}


/**
 * Convert std_logic_vector into an unsigned long
 **/
unsigned long slv2ulong_high(mtiSignalIdT stdvec)
{
	mtiSignalIdT *  elem_list;
	mtiTypeIdT      sigtype;
	unsigned long   i,num_elems;
	unsigned long   retvalue = 0; 

	sigtype = mti_GetSignalType(stdvec);  // signal type
	num_elems = mti_TickLength(sigtype);  // Get number of elements

	elem_list = mti_GetSignalSubelements(stdvec, 0);

	for ( i=0; i<64; i++ ) {
		if (mti_GetSignalValue(elem_list[i])==3)
			retvalue += ((unsigned long)(1)<<(63-i));
	}

	mti_VsimFree(elem_list);
	return(retvalue);
}


/**
 * Convert std_logic_vector into an unsigned long
 **/
unsigned long slv2ulong_low(mtiSignalIdT stdvec)
{
	mtiSignalIdT *  elem_list;
	mtiTypeIdT      sigtype;
	unsigned long   i,num_elems;
	unsigned long   retvalue = 0; 

	sigtype = mti_GetSignalType(stdvec);  // signal type
	num_elems = mti_TickLength(sigtype);  // Get number of elements

	elem_list = mti_GetSignalSubelements(stdvec, 0);

	for ( i=64; i<128; i++ ) {
		if (mti_GetSignalValue(elem_list[i])==3)
			retvalue += ((unsigned long)(1)<<(63-i));
	}

	mti_VsimFree(elem_list);
	return(retvalue);
}


/*
 * set_bar: 
 * e.g. value=1 -> bar=1111101, value=0 -> bar=1111110, ...
 **/
void set_bar( mtiDriverIdT *drivers, unsigned int value )
{
	int i;
	for (i=0;i<7;i++) {
		if ( ( (~(1<<value)) >>i) & 0x01 )
			mti_ScheduleDriver( drivers[6-i], STD_LOGIC_1, 0, MTI_INERTIAL);
		else
			mti_ScheduleDriver( drivers[6-i], STD_LOGIC_0, 0, MTI_INERTIAL);
	}	
}


/**
 * set_slv8: drive std_logic_vector(7 downto 0) with binary
 * representation of unsigned char
 **/
void set_slv8( mtiDriverIdT *drivers, unsigned char value )
{
	int i;
	for (i=0;i<8;i++) {
		if ( (value>>i) & 0x01 )
			mti_ScheduleDriver( drivers[7-i], STD_LOGIC_1, 0, MTI_INERTIAL);
		else
			mti_ScheduleDriver( drivers[7-i], STD_LOGIC_0, 0, MTI_INERTIAL);
	}
}



/**
 * set_slv32: drive std_logic_vector(31 downto 0) with binary
 * representation of unsigned int
 **/
void set_slv32( mtiDriverIdT *drivers, unsigned int value )
{
	int i;
	for (i=0;i<32;i++) {
		if ( (value>>i) & 0x01 )
			mti_ScheduleDriver( drivers[31-i], STD_LOGIC_1, 0, MTI_INERTIAL);
		else
			mti_ScheduleDriver( drivers[31-i], STD_LOGIC_0, 0, MTI_INERTIAL);
	}
}


/**
 * set_slv: drive std_logic_vector of any bit bitdth <=32 with binary
 * representation of the input data
 * */
void set_slv( mtiDriverIdT *drivers, unsigned int data, int bit_width)
{
	int i;
	for ( i=0; i<bit_width; i++ ) {
		if(data>>i & 0x01 )
			mti_ScheduleDriver( drivers[bit_width-1-i], 
					STD_LOGIC_1, 0, MTI_INERTIAL);
		else
			mti_ScheduleDriver( drivers[bit_width-1-i], 
					STD_LOGIC_0, 0, MTI_INERTIAL);
	}
}

		
		


/**
 * Load Done Callback: gets called after load phase
 **/
void loadDoneCB(void * param) // Add socket callback
{
	//inst_rec * ip = (inst_rec *)param;
	mti_PrintMessage("Load Done: Adding socket callback.\n");
	//mti_AddSocketInputReadyCB(ip->sdsc_acc, (mtiVoidFuncPtrT)sockCB, param);
	//mti_AddInputReadyCB(ip->sdsc_acc, (mtiVoidFuncPtrT)sockCB, param);
}

#endif
