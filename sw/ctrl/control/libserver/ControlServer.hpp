// $Id: ControlServer.hpp 24 2013-08-13 18:12:56Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#ifndef included_CbmNet_ControlServer
#define included_CbmNet_ControlServer 1

#include <string>
#include <vector>

#include "boost/thread/thread.hpp"
#include "zmq.hpp"

#include "libutil/ReventFd.hpp"
#include "ServSeqItem.hpp"

namespace CbmNet {

  class ControlServer {
    public:
                   ControlServer(zmq::context_t&  zmq_context, std::string driver_path);
      virtual      ~ControlServer();

      void          Bind(const std::string& cserv_bind="");
      void          ConnectDriver();

      void          Start();
      void          Stop();
      bool          IsRunning()
                      { return fServerThread.get_id() != boost::thread::id(); }

      void          SetDebugFlags(uint32_t flags)
                      { fDbgFlags = flags; }
      uint32_t      DebugFlags() const
                      { return fDbgFlags; }

    // some constants (also defined in cpp)
      static const uint32_t kDbgDumpRpc    = 0x00000010;
      static const uint32_t kDbgDumpCbmNet = 0x00000020;
      static const uint32_t kDbgLoopBack   = 0x10000000;

      static const uint16_t kProtV10_NCmd    = 0x0100;
      static const uint16_t kProtV10_PutCmd  = 0x0200;
      static const uint16_t kProtV10_GetCmd  = 0x0400;
      static const uint16_t kProtV10_PutAck  = 0x0a00;
      static const uint16_t kProtV10_GetAck  = 0x0c00;
      static const uint16_t kProtV10_NAck    = 0x1000;

    protected:
      void          ServerLoop();

      uint32_t      DoListParSeqTmp(const uint32_t* preq, 
                                    const uint32_t* preq_end, 
                                    std::vector<uint32_t>& res);
      uint32_t      DoListParSeq(const uint32_t* preq, 
                                 const uint32_t* preq_end, 
                                 std::vector<uint32_t>& res);

      void          AddServSeqItem(std::vector<ServSeqItem>& list, 
                                   uint32_t nodeid, uint32_t nseq, 
                                   const uint32_t* pidata, uint32_t* podata);

      virtual uint32_t ProcessRequest(std::vector<ServSeqItem>& list);
      void          ProcessSeqItemLoopBack(ServSeqItem& item);
      void          ProcessSeqItemSimple(ServSeqItem& item);
      uint32_t      ProcessSendDLM(const uint32_t* preq, 
                                   const uint32_t* preq_end);
      int           PollDriverRes(long timeout);

      void          DumpRpc(const std::string& text, uint32_t* p, size_t n);
      void          DumpCbmNet(const std::string& text, uint16_t* p, size_t n);
    
    uint32_t ProcessFlibRead(const uint32_t* preq, 
                             const uint32_t* preq_end,
                             std::vector<uint32_t>& res);

    uint32_t ProcessFlibWrite(const uint32_t* preq, 
                             const uint32_t* preq_end);

    protected:
      zmq::context_t&   fZcontext;
      std::string       fDriverPath;
      zmq::socket_t     fZsocketClient;
      zmq::socket_t     fZsocketCntlDriverReq;
      zmq::socket_t     fZsocketCntlDriverRes;
      CbmNet::ReventFd  fWakeEvent;
      boost::thread     fServerThread;          //!< driver thread
      uint32_t          fDbgFlags;
};
  
} // end namespace CbmNet

#endif
