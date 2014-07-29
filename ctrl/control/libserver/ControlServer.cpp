// $Id: ControlServer.cpp 25 2013-08-14 19:10:24Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "libutil/Util.hpp"
#include "libutil/RlogFileDefault.hpp"
#include "libutil/RlogMsg.hpp"
#include "libutil/RosPrintf.hpp"
#include "control/ControlProtocol.hpp"
#include "control/libclient/ListSeq.hpp"
#include "ControlServer.hpp"
#include "ServSeqItem.hpp"

using namespace std;

// all method definitions in namespace CbmNet
namespace CbmNet {

//------------------------------------------+-----------------------------------
// constants definitions

const uint32_t ControlServer::kDbgDumpRpc;
const uint32_t ControlServer::kDbgDumpCbmNet;
const uint32_t ControlServer::kDbgLoopBack;

const uint16_t ControlServer::kProtV10_NCmd;
const uint16_t ControlServer::kProtV10_PutCmd;
const uint16_t ControlServer::kProtV10_GetCmd;
const uint16_t ControlServer::kProtV10_PutAck;
const uint16_t ControlServer::kProtV10_GetAck;
const uint16_t ControlServer::kProtV10_NAck;
  
//------------------------------------------+-----------------------------------
//! Default constructor

  ControlServer::ControlServer(zmq::context_t&  zmq_context, std::string driver_path)
  : fZcontext(zmq_context),
    fDriverPath(driver_path),
    fZsocketClient(fZcontext, ZMQ_REP),
    fZsocketCntlDriverReq(fZcontext, ZMQ_PUSH),
    fZsocketCntlDriverRes(fZcontext, ZMQ_PULL),
    fWakeEvent(),
    fServerThread(),
    fDbgFlags(0)
{}

//------------------------------------------+-----------------------------------
//! Destructor

ControlServer::~ControlServer()
{
  Stop();
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ControlServer::Bind(const std::string& cserv_bind)
{
  string bpath = cserv_bind;
  if (bpath.size() == 0) {
    ostringstream oss;
    oss << "tcp://*:";
    oss << kPortControl;
    bpath = oss.str();
  }
  fZsocketClient.bind(bpath.c_str());
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ControlServer::ConnectDriver()
{
  std::string req = "inproc://" + fDriverPath + "req";
  std::string res = "inproc://" + fDriverPath + "res";
  fZsocketCntlDriverReq.connect(req.c_str());
  fZsocketCntlDriverRes.connect(res.c_str());
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ControlServer::Start() 
{
  if (IsRunning()) Stop();
  fServerThread =  boost::thread(boost::bind(&ControlServer::ServerLoop, this));
  return;
}  

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ControlServer::Stop() 
{
  if (!IsRunning()) return;
  fWakeEvent.Signal();
  if (!fServerThread.timed_join(boost::posix_time::milliseconds(500))) {
    throw runtime_error("ControlServer::Stop(): server thread failed to stop");
  }
  return;
}  

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ControlServer::ServerLoop()
{
  zmq_pollitem_t  pollitems[2];
  pollitems[0].socket = 0;
  pollitems[0].fd     = fWakeEvent.Fd();
  pollitems[0].events = ZMQ_POLLIN;
  pollitems[1].socket = fZsocketClient;
  pollitems[1].fd     = 0;
  pollitems[1].events = ZMQ_POLLIN;  

  while(true) {
    int irc = ::zmq_poll(pollitems, 2, 1000);
    if (irc==-1 && errno==EINTR) continue;
    
    if (irc < 0) 
      throw runtime_error("ControlServer::ServerLoop(): zmq_poll() failed");

    // check for wakeup event, if seen simply end server loop
    if (pollitems[0].revents) break;

    // check for surious zmq event
    if (pollitems[1].revents == 0) continue;
    
    // handle zmq request
    zmq::message_t reqmsg;
    fZsocketClient.recv(&reqmsg);
    uint32_t* preq = (uint32_t*)reqmsg.data();
    size_t    nreq = reqmsg.size()/sizeof(uint32_t);
    uint32_t* preq_end = preq + nreq;

    if (fDbgFlags & kDbgDumpRpc) DumpRpc("request", preq, nreq);

    vector<uint32_t> res;
    uint32_t mstat =  kStatusFailRMC;

    // method dispatcher
    if (preq < preq_end) {
      uint32_t meth = *preq++;
      switch(meth) {
      case kMethodGetBOM:
        mstat = 0x3210;
        break;
      case kMethodDoListParSeq:
        mstat = DoListParSeq(preq, preq_end, res);
        break;
      case kMethodDoListParSeqTmp:
        mstat = DoListParSeqTmp(preq, preq_end, res);
        break;
      case kMethodSendDLM:
        mstat = ProcessSendDLM(preq, preq_end);
        break;
      case kMethodFlibRead:
        mstat = ProcessFlibRead(preq, preq_end, res);
        break;
      case kMethodFlibWrite:
        mstat = ProcessFlibWrite(preq, preq_end);
        break;

      default:
        break;
      }
    }

    // finally store method status in first response word
    if (res.size() == 0) res.resize(1);
    res[0] = mstat;

    if (fDbgFlags & kDbgDumpRpc) DumpRpc("response", res.data(), res.size());

    fZsocketClient.send(res.data(), sizeof(uint32_t)*res.size());
  }

  // comes here when server thread stops
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

uint32_t ControlServer::DoListParSeqTmp(const uint32_t* preq, 
                                        const uint32_t* preq_end, 
                                        std::vector<uint32_t>& res)
{
  if (preq >= preq_end) return kStatusFailRMC;
  size_t nseq = *preq++;
  const uint32_t* ptmp = preq;
  preq += 2*nseq;

  if (preq >= preq_end) return kStatusFailRMC;
  size_t npar = *preq++;
  if (preq+npar >= preq_end) return kStatusFailRMC;

  // now build ServSeqItem list and response vector
  vector<ServSeqItem> list;
  list.reserve(npar);
  res.resize(1+npar*(2+nseq));
  uint32_t* pres = &res[1];

  for (size_t ipar=0; ipar<npar; ipar++) {
    uint32_t nodeid = *preq++;
    AddServSeqItem(list, nodeid, nseq, ptmp, pres);
    pres += 2+nseq;
  }

  return ProcessRequest(list);
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

uint32_t ControlServer::DoListParSeq(const uint32_t* preq, 
                                     const uint32_t* preq_end, 
                                     std::vector<uint32_t>& res)
{
  if (preq >= preq_end) return kStatusFailRMC;
  size_t npar = *preq++;

  // check request, calculate response vector length
  size_t nres = 0;
  const uint32_t* p = preq;
  for (size_t ipar=0; ipar<npar; ipar++) {
    if (p+2 > preq_end) return kStatusFailRMC;
    size_t nseq = p[1];
    if (p+2+2*nseq > preq_end) return kStatusFailRMC;
    nres += 2 +   nseq;
    p    += 2 + 2*nseq;
  }

  // now build ServSeqItem list and response vector
  vector<ServSeqItem> list;
  list.reserve(npar);
  res.resize(1+nres);
  uint32_t* pres = &res[1];

  for (size_t ipar=0; ipar<npar; ipar++) {
    uint32_t nodeid = *preq++;
    size_t nseq     = *preq++;
    AddServSeqItem(list, nodeid, nseq, preq, pres);
    pres += 2+nseq;
    preq += 2*nseq;
  }

  return ProcessRequest(list);
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ControlServer::AddServSeqItem(vector<ServSeqItem>& list, 
                                   uint32_t nodeid, uint32_t nseq, 
                                   const uint32_t* pidata, uint32_t* podata)
{
  ServSeqItem item;
  uint16_t cbmnetid = nodeid;
  uint16_t protvers = 0;
  uint16_t nopermax = 1;
  item.InitNodeInfo(nodeid,  cbmnetid, protvers, nopermax);
  item.InitDataInfo(nseq, pidata, podata);

  list.push_back(item);

  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

uint32_t ControlServer::ProcessRequest(std::vector<ServSeqItem>& list)
{
  for (size_t ipar=0; ipar<list.size(); ipar++) {
    ServSeqItem& item(list[ipar]);
    if (fDbgFlags & kDbgLoopBack) {
      ProcessSeqItemLoopBack(item);
    } else {
      ProcessSeqItemSimple(item);
    }
  }
  return kStatusOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ControlServer::ProcessSeqItemLoopBack(ServSeqItem& item)
{
  item.SetStatus(kStatusOK);
  uint32_t nseq = item.NOper();
  item.SetNDone(nseq);
  uint32_t value = 0xbeaf0000;
  for (size_t iseq=0; iseq<nseq; iseq++) {
    if (item.Type(iseq)==kOperRead) item.SetValue(iseq,value++);
  }
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ControlServer::ProcessSeqItemSimple(ServSeqItem& item)
{
  item.SetStatus(kStatusOK);
  item.SetNDone(0);

  uint16_t cbmnet_targ_addr = item.Cbmnetid() & 0xffff;
  uint16_t cbmnet_host_addr = 0x0000;

  vector<uint16_t> req;
  zmq::message_t resmsg;

  uint32_t nseq = item.NOper();

  for (size_t iseq=0; iseq<nseq; iseq++) {
    uint32_t device_addr = item.Addr(iseq);
    uint16_t device_addr_msb = (device_addr>>16) & 0x00ff;
    uint16_t device_addr_lsb = device_addr & 0xffff;
    uint16_t device_cmd = 
      item.Type(iseq)==kOperWrite ? kProtV10_PutCmd : kProtV10_GetCmd;

    // request format
    // [0]  ROCID
    // [1]  RETID
    // [2]  Ncmd.msb
    // [3]  Ncmd.lsb
    // [4]  addr.msb  
    // [5]  addr.lsb  
    // [6]  data.msb  (only for write)
    // [7]  data.lsb
    req.clear();
    req.push_back(cbmnet_targ_addr);        // ROCID
    req.push_back(cbmnet_host_addr);        // RETID
    req.push_back(kProtV10_NCmd);           // N cmd 
    req.push_back(1);                       // N=1
    req.push_back(device_cmd | device_addr_msb);
    req.push_back(device_addr_lsb);

    if (item.Type(iseq)==kOperWrite) {
      uint32_t device_data = item.Value(iseq);
      uint16_t device_data_msb = (device_data>>16) & 0xffff;
      uint16_t device_data_lsb = device_data & 0xffff;
      req.push_back(device_data_msb);
      req.push_back(device_data_lsb);      
    }

    while(PollDriverRes(0) > 0) {
      fZsocketCntlDriverRes.recv(&resmsg);
      RlogMsg lmsg(gRlogFile,'+');
      lmsg << "ControlServer::ProcessSeqItemSimple: spurious packet dropped";
    }

    if (fDbgFlags & kDbgDumpCbmNet) 
      DumpCbmNet("send", req.data(), req.size());

    fZsocketCntlDriverReq.send(req.data(), sizeof(uint16_t)*req.size());

    if (PollDriverRes(1000) > 0) {
      fZsocketCntlDriverRes.recv(&resmsg);
      uint16_t* pres = (uint16_t*)resmsg.data();
      size_t    nres = resmsg.size()/sizeof(uint16_t);
      if (fDbgFlags & kDbgDumpCbmNet) 
        DumpCbmNet("received", pres, nres);

      if (nres == 0) {
        item.SetStatus(kStatusFailTOutHard);
        break;
      } else if (nres != 6) {
        item.SetStatus(kStatusFail);
        RlogMsg lmsg(gRlogFile,'+');
        lmsg << "ControlServer::ProcessSeqItemSimple: response length bad";
        break;
      }
      // response format
      // [0]  RETID
      // [1]  ROCID
      // [2]  addr.msb  
      // [3]  addr.lsb  
      // [4]  data.msb  
      // [5]  data.lsb

      uint16_t device_res = pres[2] & 0xff00;
      if (device_res == kProtV10_NAck) {    // NACK seen
        item.SetStatus(kStatusFailNack);
        break;
      }
      item.SetNDone(iseq+1);
      if (item.Type(iseq)==kOperRead) {
        uint32_t device_data_msb = pres[4];
        uint32_t device_data_lsb = pres[5];
        item.SetValue(iseq, device_data_msb<<16 | device_data_lsb);
      }

    } else {
      item.SetStatus(kStatusFailTOutSoft);
      RlogMsg lmsg(gRlogFile,'+');
      lmsg << "ControlServer::ProcessSeqItemSimple: response timeout";
      break;
    }

  }
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

uint32_t ControlServer::ProcessSendDLM(const uint32_t* preq, 
                                       const uint32_t* preq_end)
{
  if (preq >= preq_end) return kStatusFailRMC;
  uint32_t dnum = *preq++;

  if (fDbgFlags & kDbgLoopBack) return kStatusOK;

  uint16_t dnum16 = dnum;
  fZsocketCntlDriverReq.send(&
dnum16, sizeof(uint16_t));
  return kStatusOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

uint32_t ControlServer::ProcessFlibRead(const uint32_t* preq, 
                                        const uint32_t* preq_end,
                                        std::vector<uint32_t>& res)
{
 
  zmq::message_t resmsg;
  res.resize(2); // response is fixed size of 32 Bit, Word0 is filled with status later

  if (preq >= preq_end) return kStatusFailRMC;

  fZsocketCntlDriverReq.send(preq, sizeof(uint32_t));

  if (PollDriverRes(1000) > 0) {
    fZsocketCntlDriverRes.recv(&resmsg);
    if (resmsg.size() > 4) return kStatusFailRMC;
    uint32_t* data = (uint32_t*)resmsg.data();
    res[1] = data[0];
  } 
  
  return kStatusOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

uint32_t ControlServer::ProcessFlibWrite(const uint32_t* preq, 
                                        const uint32_t* preq_end)
{
 
  if (preq >= preq_end) return kStatusFailRMC;

  fZsocketCntlDriverReq.send(preq, 2*sizeof(uint32_t));
  
  return kStatusOK;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

int ControlServer::PollDriverRes(long timeout)
{
  zmq_pollitem_t  pollitems[1];
  pollitems[0].socket = fZsocketCntlDriverRes;
  pollitems[0].fd     = 0;
  pollitems[0].events = ZMQ_POLLIN;  

  while(true) {
    int irc = ::zmq_poll(pollitems, 1, timeout);
    if (irc==-1 && errno==EINTR) continue;
    if (irc < 0) 
      throw runtime_error("ControlServer::PollDriverRes(): zmq_poll() failed");
    return irc;
  }
  
  return -1;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ControlServer::DumpRpc(const std::string& text, uint32_t* p, size_t n)
{
  RlogMsg lmsg(gRlogFile,'+');
  lmsg <<  "Rpc Message " << text << ", length=" << n << endl;
  for (size_t i=0; i<n; i++) {
    lmsg << "  " << RosPrintf(i,"d",4)
         << " : " << RosPrintf(p[i],"x0",8)
         << " (" << RosPrintf(p[i],"d",10) << ")"
         << endl;
  }
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ControlServer::DumpCbmNet(const std::string& text, uint16_t* p, size_t n)
{
  RlogMsg lmsg(gRlogFile,'+');
  lmsg <<  "CbmNet Message " << text << ", length=" << n << endl;
  for (size_t i=0; i<n; i++) {
    if (i%8 == 0) {
      if (i != 0) lmsg << "\n";
      lmsg << "    " << RosPrintf(i,"x0",4) << ": ";
    }
    lmsg << RosPrintf(p[i],"x0",4) << " ";
  }
  return;
}


} // end namespace CbmNet
