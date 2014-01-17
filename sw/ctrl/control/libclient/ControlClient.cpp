// $Id: ControlClient.cpp 24 2013-08-13 18:12:56Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <iostream>

#include "libutil/Util.hpp"
#include "control/ControlProtocol.hpp"
#include "ControlClient.hpp"

using namespace std;

// all method definitions in namespace CbmNet
namespace CbmNet {

//------------------------------------------+-----------------------------------
//! Default constructor

ControlClient::ControlClient()
  : fZcontext(1),
    fZsocket(fZcontext, ZMQ_REQ)
{}

//------------------------------------------+-----------------------------------
//! Destructor

ControlClient::~ControlClient()
{
  Close();
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ControlClient::Connect(const std::string& dserv_path)
{
  fZsocket.connect(dserv_path.c_str());
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ControlClient::Close()
{
  fZsocket.close();
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::Read(uint32_t nodeid, uint32_t addr, uint32_t& value)
{
  ListSeq list;
  list.AddRead(addr);
  int rc = DoListSeq(nodeid, list);
  value = list[0].value;
  return rc;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::Write(uint32_t nodeid, uint32_t addr, uint32_t value)
{
  ListSeq list;
  list.AddWrite(addr, value);
  int rc = DoListSeq(nodeid, list);
  return rc;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::ReadSeq(uint32_t nodeid, uint32_t addr,
                           std::vector<uint32_t>& values, size_t nval,
                           uint32_t& status, int32_t addrinc)
{
  values.resize(nval);
  int rc = ReadSeq(nodeid, addr, values.data(), nval, status, addrinc);
  values.resize(rc>0 ? rc : 0);
  return rc;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::ReadSeq(uint32_t nodeid, uint32_t addr, 
                           uint32_t* pvalues, size_t nval,
                           uint32_t& status, int32_t addrinc)
{
  ListSeq list;
  list.AddReadBlock(addr, nval, addrinc);
  int rc = DoListSeq(nodeid, list);
  list.GetReadBlock(pvalues);
  status = list.Status();
  return rc;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::WriteSeq(uint32_t nodeid, uint32_t addr, 
                            const std::vector<uint32_t>& values,
                            uint32_t& status, int32_t addrinc)
{
  return WriteSeq(nodeid, addr, values.data(), values.size(), status, addrinc);
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::WriteSeq(uint32_t nodeid, uint32_t addr, 
                            const uint32_t* pvalues, size_t nval,
                            uint32_t& status, int32_t addrinc)
{
  ListSeq list;
  list.AddWriteBlock(addr, pvalues, nval, addrinc);
  int rc = DoListSeq(nodeid, list);
  status = list.Status();
  return rc;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::ReadPar(const std::vector<uint32_t>& nodeids, 
                           uint32_t addr, std::vector<uint32_t>& values,
                           std::vector<uint32_t>& status)
{
  values.resize(nodeids.size());
  status.resize(nodeids.size());
  int rc = ReadPar(nodeids.data(), addr, values.data(), status.data(),
                   nodeids.size());
  return rc;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::ReadPar(const uint32_t* pnodeids, uint32_t addr, 
                           uint32_t* pvalues, uint32_t* pstatus, 
                           size_t npar)
{
  ListPar list;
  for (size_t i=0; i<npar; i++) list.AddRead(*pnodeids++, addr);
  int rc = DoListPar(list);
  list.GetValues(pvalues);
  list.GetStatus(pstatus);
  return rc;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::WritePar(const std::vector<uint32_t>& nodeids, 
                            uint32_t addr, uint32_t value,
                            std::vector<uint32_t>& status)
{
  status.resize(nodeids.size());
  int rc = WritePar(nodeids.data(), addr, value, status.data(),
                    nodeids.size());
  return rc;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::WritePar(const uint32_t* pnodeids, uint32_t addr, 
                            uint32_t value, uint32_t* pstatus,
                            size_t npar)
{
  ListPar list;
  for (size_t i=0; i<npar; i++) list.AddWrite(*pnodeids++, addr, value);
  int rc = DoListPar(list);
  list.GetStatus(pstatus);
  return rc;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::WritePar(const std::vector<uint32_t>& nodeids, 
                            uint32_t addr, 
                            const std::vector<uint32_t>& values,
                            std::vector<uint32_t>& status)
{
  status.resize(nodeids.size());
  int rc = WritePar(nodeids.data(), addr, values.data(), status.data(),
                    nodeids.size());
  return rc;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::WritePar(const uint32_t* pnodeids, uint32_t addr, 
                            const uint32_t* pvalues, uint32_t* pstatus,
                            size_t npar)
{
  ListPar list;
  for (size_t i=0; i<npar; i++) list.AddWrite(*pnodeids++, addr, *pvalues++);
  int rc = DoListPar(list);
  list.GetStatus(pstatus);
  return rc;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::DoListSeq(uint32_t nodeid, ListSeq& list)
{
  list.SetNodeid(nodeid);
  int rc = DoListParSeq(&list, 1);
  return (rc == 1) ? list.NDone() : rc;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::DoListPar(ListPar& list)
{
  // currently simply map in the most simple way to DoListParSeq
  // can later be implemented in an optimized way

  vector<ListSeq> parseqlist;
  size_t npar = list.Size();
  parseqlist.resize(npar);
  for (size_t ipar=0; ipar<npar; ipar++) {
    ListParItem& paritem(list[ipar]);
    ListSeq&     seqlist(parseqlist[ipar]);
    seqlist.SetNodeid(paritem.nodeid);
    seqlist.AddOper(paritem.type, paritem.addr, paritem.value);
  }

  int rc = DoListParSeq(parseqlist);
  if (rc <= 0) return rc;

  for (size_t ipar=0; ipar<npar; ipar++) {
    ListParItem& paritem(list[ipar]);
    ListSeq&     seqlist(parseqlist[ipar]);
    paritem.status = seqlist.Status();
    if (paritem.status == 0 && seqlist[1].type == kOperRead) {
      paritem.value  = seqlist[1].value;
    }
  }

  return rc;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::DoListParSeq(std::vector<CbmNet::ListSeq>& lists)
{
  return DoListParSeq(lists.data(), lists.size());
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::DoListParSeq(ListSeq* plists, size_t npar)
{
  // request vector format
  //   method number
  //   npar
  //   .. npar times ..
  //     nodeid
  //     nseq
  //     .. nseq times ..
  //       type+addr
  //       value

  size_t nval = 0;
  for (size_t i=0; i<npar; i++) {
    ListSeq& list(plists[i]);
    list.SetStatus(kStatusNotYet);
    list.SetNDone(0);
    nval += 2 + 2*list.Size();
  }

  vector<uint32_t> req;
  req.reserve(2+nval);                      // reserve to avoid reallocs

  req.push_back(kMethodDoListParSeq);
  req.push_back(npar);

  for (size_t i=0; i<npar; i++) {
    ListSeq& list(plists[i]);
    size_t nseq = list.Size();
    req.push_back(list.Nodeid());
    req.push_back(nseq);
    for (size_t j=0; j<nseq; j++) {
      ListSeqItem& item(list[j]);
      req.push_back(MergeTypeAddr(item.type, item.addr));
      req.push_back(item.value);
    }
  }

  fZsocket.send(req.data(), sizeof(uint32_t)*req.size());
  zmq::message_t resmsg;
  fZsocket.recv(&resmsg);

  return HandleParSeqResponse(plists, npar, resmsg);
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::DoListParSeq(const std::vector<uint32_t>& nodeids, 
                                const ListSeq& listreq, 
                                std::vector<CbmNet::ListSeq>& listres)
{
  listres.resize(nodeids.size());
  return DoListParSeq(nodeids.data(), listreq, listres.data(), nodeids.size());
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::DoListParSeq(const uint32_t* pnodeids, 
                                const ListSeq& listtmp, 
                                ListSeq* plistres, size_t npar)
{
  for (size_t i=0; i<npar; i++) {
    ListSeq& list(plistres[i]);
    list = listtmp;
    list.SetStatus(kStatusNotYet);
    list.SetNDone(0);
    list.SetNodeid(pnodeids[i]);
  }

  size_t nseq = listtmp.Size();

  // request vector format
  //   method number
  //   nseq
  //   .. nseq times ..
  //     tpye+addr
  //     value
  //   npar
  //   .. npar times ..
  //     nodeid

  vector<uint32_t> req;
  req.reserve(2+2*nseq+npar);               // reserve to avoid reallocs
  
  req.push_back(kMethodDoListParSeqTmp);
  req.push_back(nseq);

  for (size_t iseq=0; iseq<nseq; iseq++) {
    const ListSeqItem& item(listtmp[iseq]);
    req.push_back(MergeTypeAddr(item.type, item.addr));
    req.push_back(item.value);
  }
  req.push_back(npar);
  for (size_t ipar=0; ipar<npar; ipar++) {
    req.push_back(pnodeids[ipar]);
  }

  fZsocket.send(req.data(), sizeof(uint32_t)*req.size());
  zmq::message_t resmsg;
  fZsocket.recv(&resmsg);

  return HandleParSeqResponse(plistres, npar, resmsg);
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

uint32_t ControlClient::ServerBom(uint32_t nodeid)
{
  uint32_t meth = kMethodGetBOM;
  fZsocket.send(&meth, sizeof(meth));
  zmq::message_t resmsg;
  fZsocket.recv(&resmsg);
  uint32_t* pres = (uint32_t*)resmsg.data();
  size_t    nres = resmsg.size()/sizeof(uint32_t);
  return nres>0 ? pres[0] : 0xffff;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::SendDLM(uint32_t nodeid, uint32_t num)
{
  vector<uint32_t> req;
  req.push_back(kMethodSendDLM);
  req.push_back(num);

  fZsocket.send(req.data(), sizeof(uint32_t)*req.size());
  zmq::message_t resmsg;
  fZsocket.recv(&resmsg);

  uint32_t* pres = (uint32_t*)resmsg.data();
  size_t    nres = resmsg.size()/sizeof(uint32_t);
  return nres>0 ? -pres[0] : -kStatusFail;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs
  int ControlClient::FlibRead(uint32_t nodeid, uint32_t addr, uint32_t& value)
  {
  vector<uint32_t> req;
  req.push_back(kMethodFlibRead);
  req.push_back(addr);
  fZsocket.send(req.data(), sizeof(uint32_t)*req.size());

  zmq::message_t resmsg;
  fZsocket.recv(&resmsg);
  uint32_t* pres = (uint32_t*)resmsg.data();
  size_t    nres = resmsg.size()/sizeof(uint32_t);
  value = pres[1];

  return nres>0 ? -pres[0] : -kStatusFail;
  }

//------------------------------------------+-----------------------------------
//! FIXME_docs
  int ControlClient::FlibWrite(uint32_t nodeid, uint32_t addr, uint32_t value)
  {
  vector<uint32_t> req;
  req.push_back(kMethodFlibWrite);
  req.push_back(addr);
  req.push_back(value);
  fZsocket.send(req.data(), sizeof(uint32_t)*req.size());

  zmq::message_t resmsg;
  fZsocket.recv(&resmsg);
  uint32_t* pres = (uint32_t*)resmsg.data();
  size_t    nres = resmsg.size()/sizeof(uint32_t);

  return nres>0 ? -pres[0] : -kStatusFail;
  }

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlClient::HandleParSeqResponse(ListSeq* plists, size_t npar,
                                        zmq::message_t& resmsg)
{
  // response vector format
  //   method status
  //   .. npar times ..
  //     sequence status
  //     ndone
  //     .. nseq times ..
  //       value

  uint32_t* pres = (uint32_t*)resmsg.data();
  size_t    nres = resmsg.size()/sizeof(uint32_t);
  uint32_t* pres_end = pres + nres;

  // handle method status
  if (pres == pres_end) return -kStatusFailRMC;
  uint32_t mstat = *pres++;
  if (mstat > 0) return -mstat;

  // loop over seq lists
  size_t npardone = 0;
  for (size_t ipar=0; ipar<npar; ipar++) {
    ListSeq& list(plists[ipar]);
    size_t nseq = list.Size();
    if (pres+2 > pres_end) break;
    uint32_t sstat = *pres++;
    uint32_t ndone = *pres++;
    if (ndone > nseq || pres+nseq > pres_end) break;
    for (size_t iseq=0; iseq<ndone; iseq++) {
      ListSeqItem& item(list[iseq]);
      if (item.type == kOperRead) item.value = pres[iseq];
    } 
    pres += nseq;
    list.SetStatus(sstat);
    list.SetNDone(ndone);
    npardone += 1;
  }

  // set fail status in all non-covered listseq's
  for (size_t ipar=npardone; ipar<npar; ipar++) {
    ListSeq& list(plists[ipar]);
    list.SetStatus(kStatusFailRMC);
  }
  
  return npardone;
}

} // end namespace CbmNet
