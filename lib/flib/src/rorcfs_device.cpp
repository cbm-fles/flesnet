/**
 *
 * @file
 * @author Dominic Eschweiler <dominic.eschweiler@cern.ch>
 * @date 2014-07-03
 *
 **/

#include<cstdlib>
#include<iostream>
#include<cstring>

#include<stdio.h>
#include<dirent.h>
#include<errno.h>
#include<sys/stat.h>

#include<pda.h>

#include "../include/flib/rorcfs_device.hh"

using namespace std;


device::device()
{
    const char *pci_ids[] =
    {
        "10dc beef", /* CRORC as registered at CERN */
        NULL         /* Delimiter*/
    };

    if( (m_dop = DeviceOperator_new(pci_ids)) == NULL)
    { throw DEVICE_CONSTRUCTOR_FAILED; }

    if(DeviceOperator_getPciDevice(m_dop, &m_device, 0) != PDA_SUCCESS)
    { throw DEVICE_CONSTRUCTOR_FAILED; }


}

device::~device()
{
    if( DeviceOperator_delete(m_dop, PDA_DELETE_PERSISTANT) != PDA_SUCCESS)
    { cout << "Deleting device operator failed!" << endl; }
}

int device::init(int n)
{
//	int n_dev;
//	char basedir[] = "/sys/module/rorcfs/drivers/pci:rorcfs/";
//	struct dirent **namelist;
//	char *pEnd;
//
//	if (dname)
//		free(dname);
//
//	n_dev = find_rorc(basedir, &namelist);
//	if ( n_dev <= 0 || n<0 || n > n_dev ) {
//          free_namelist(namelist, n_dev);
//          return -1;
//        }
//
//	bus = (uint8) strtoul(namelist[n]->d_name+5, &pEnd, 16);
//	slot = (uint8) strtoul(pEnd+1, &pEnd, 16);
//	func= (uint8) strtoul(pEnd+1, &pEnd, 16);
//
//	dname_size = snprintf(NULL, 0, "%s0000:%02x:%02x.%x/mmap/",
//			basedir, bus, slot, func);
//	dname_size++;
//
//	dname = (char *) malloc(dname_size);
//	if ( !dname ) {
//          free_namelist(namelist, n_dev);
//          return -1;
//        }
//	snprintf(dname, dname_size, "%s0000:%02x:%02x.%x/mmap/",
//			basedir, bus, slot, func);
//
//        free_namelist(namelist, n_dev);
//	return 0;
//}
//
//void device::free_namelist(struct dirent **namelist, int n) {
//  if (n > 0) {
//    while (n--) {
//      free(namelist[n]);
//    }
//    free(namelist);
//  }
  return 0;
}
