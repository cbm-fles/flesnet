// $Id: ControlClient.hpp 24 2013-08-13 18:12:56Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#ifndef included_CbmNet_ControlClient
#define included_CbmNet_ControlClient 1

#include <cstddef>
//#include <cstdint>  --> avoid, needs -std=c++0x -> rootcint problems
#include <stdint.h>
#include <vector>

#include "zmq.hpp"

#include "ListSeq.hpp"
#include "ListPar.hpp"

namespace CbmNet {

  class ControlClient {
    public:
                    ControlClient();
      virtual      ~ControlClient();

      void          Connect(const std::string& dserv_path);
      void          Close();

      int           Read(uint32_t nodeid, uint32_t addr, uint32_t& value);
      int           Write(uint32_t nodeid, uint32_t addr, uint32_t value);

      int           ReadSeq(uint32_t nodeid, uint32_t addr, 
                            std::vector<uint32_t>& values, size_t nval,
                            uint32_t& status, int32_t addrinc=0);
      int           ReadSeq(uint32_t nodeid, uint32_t addr, 
                            uint32_t* pvalues, size_t nval,
                            uint32_t& status, int32_t addrinc=0);

      int           WriteSeq(uint32_t nodeid, uint32_t addr, 
                             const std::vector<uint32_t>& values,
                             uint32_t& status, int32_t addrinc=0);
      int           WriteSeq(uint32_t nodeid, uint32_t addr, 
                             const uint32_t* pvalues, size_t nval,
                             uint32_t& status, int32_t addrinc=0);

      int           ReadPar(const std::vector<uint32_t>& nodeids, 
                            uint32_t addr, std::vector<uint32_t>& values,
                            std::vector<uint32_t>& status);
      int           ReadPar(const uint32_t* pnodeids, uint32_t addr, 
                            uint32_t* pvalues, uint32_t* pstatus, 
                            size_t npar);

      int           WritePar(const std::vector<uint32_t>& nodeids, 
                             uint32_t addr, uint32_t value,
                             std::vector<uint32_t>& status);
      int           WritePar(const uint32_t* pnodeids, uint32_t addr, 
                             uint32_t value, uint32_t* pstatus,
                             size_t npar);

      int           WritePar(const std::vector<uint32_t>& nodeids, 
                             uint32_t addr, 
                             const std::vector<uint32_t>& values,
                             std::vector<uint32_t>& status);
      int           WritePar(const uint32_t* pnodeids, uint32_t addr, 
                             const uint32_t* pvalues, uint32_t* pstatus,
                             size_t npar);

      int           DoListSeq(uint32_t nodeid, ListSeq& list);
      int           DoListPar(ListPar& list);

    // explict CbmNet::ListSeq used in vector<> to make rootcint happy
      int           DoListParSeq(std::vector<CbmNet::ListSeq>& lists);
      int           DoListParSeq(const std::vector<uint32_t>& nodeids, 
                                 const ListSeq& listreq, 
                                 std::vector<CbmNet::ListSeq>& listres);

      int           DoListParSeq(ListSeq* plists, size_t npar);
      int           DoListParSeq(const uint32_t* pnodeids, 
                                 const ListSeq& listtmp, 
                                 ListSeq* plistres, size_t npar);

      uint32_t      ServerBom(uint32_t nodeid);
    
    // this is currently a hack, will most likely be removed later
      int           SendDLM(uint32_t nodeid, uint32_t num);

    int FlibRead(uint32_t nodeid, uint32_t addr, 
                                uint32_t& value);

    int FlibWrite(uint32_t nodeid, uint32_t addr, 
                                uint32_t value);

 
    protected:
      int           HandleParSeqResponse(ListSeq* plists, size_t npar,
                                         zmq::message_t& resmsg);

    protected:
      zmq::context_t  fZcontext;
      zmq::socket_t   fZsocket;
};
  
} // end namespace CbmNet

#endif
