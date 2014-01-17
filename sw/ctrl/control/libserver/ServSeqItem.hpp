// $Id: ServSeqItem.hpp 15 2013-08-06 16:20:18Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#ifndef included_CbmNet_ServSeqItem
#define included_CbmNet_ServSeqItem 1

#include <cstdint>

#include "libutil/Util.hpp"

namespace CbmNet {

  class ServSeqItem {
    public:
                    ServSeqItem();
                   ~ServSeqItem();

      void          InitNodeInfo(uint32_t nodeid, uint16_t cbmnetid, 
                                 uint16_t protvers, uint16_t nopermax);
    
      void          InitDataInfo(uint32_t noper, const uint32_t* pidata, 
                                 uint32_t* podata);
    
      uint32_t      Nodeid() const
                      { return fNodeid; }
      uint32_t      Cbmnetid() const
                      { return fCbmnetid; }
      uint16_t      ProtVersion() const
                      { return fProtVersion; }
      uint16_t      NOperMax() const
                      { return fNOpermax; }
      uint32_t      NOper() const
                      { return fNOper; }
      const uint32_t* IData() const
                      { return fpIData; }
      uint32_t*     OData() const
                      { return fpOData; }
      uint32_t      TAddr(size_t i) const
                      { return fpIData[2*i]; }
      uint32_t      Type(size_t i) const
                      { return GetTypeFromTAddr(TAddr(i)); }
      uint32_t      Addr(size_t i) const
                      { return GetAddrFromTAddr(TAddr(i)); }
      uint32_t      Value(size_t i) const
                      { return fpIData[2*i+1]; }
      void          SetStatus(uint32_t status) const
                      { *fpStatus = status; }
      void          SetNDone(uint32_t ndone) const
                      { *fpNDone = ndone; }
      void          SetValue(size_t i, uint32_t val) const
                      { fpOData[i] = val; }

    protected:
      uint32_t      fNodeid;
      uint32_t      fCbmnetid;
      uint16_t      fProtVersion;
      uint16_t      fNOpermax;
      uint32_t      fNOper;
      const uint32_t* fpIData;
      uint32_t*     fpStatus;
      uint32_t*     fpNDone;
      uint32_t*     fpOData;
};
  
} // end namespace CbmNet

#endif
