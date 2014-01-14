// $Id: ServSeqItem.cpp 16 2013-08-08 09:48:02Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#include "control/ControlProtocol.hpp"

#include "ServSeqItem.hpp"

using namespace std;

// all method definitions in namespace CbmNet
namespace CbmNet {

//------------------------------------------+-----------------------------------
//! Default constructor

ServSeqItem::ServSeqItem()
  : fNodeid(0), 
    fCbmnetid(0), 
    fProtVersion(0), 
    fNOpermax(0), 
    fNOper(0), 
    fpIData(0), 
    fpStatus(0), 
    fpNDone(0), 
    fpOData(0)
{}

//------------------------------------------+-----------------------------------
//! Destructor

ServSeqItem::~ServSeqItem()
{}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ServSeqItem::InitNodeInfo(uint32_t nodeid, uint16_t cbmnetid, 
                               uint16_t protvers, uint16_t nopermax)
{ 
  fNodeid      = nodeid;
  fCbmnetid    = cbmnetid;
  fProtVersion = protvers;
  fNOpermax    = nopermax;
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ServSeqItem::InitDataInfo(uint32_t noper, const uint32_t* pidata, 
                                 uint32_t* podata)
{ 
  fNOper   = noper;
  fpIData  = pidata;
  fpStatus = podata++;
  fpNDone  = podata++;
  fpOData  = podata;

  *fpStatus = kStatusFail;
  *fpNDone  = 0;

  for (size_t i=0; i<noper; i++) *podata++ = 0xdeadbeaf;

  return;
}



} // end namespace CbmNet
