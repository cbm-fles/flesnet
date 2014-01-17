// $Id: ControlProtocol.hpp 24 2013-08-13 18:12:56Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#ifndef included_CbmNet_ControlProtocol
#define included_CbmNet_ControlProtocol 1

namespace CbmNet {

  enum OperType       {kOperRead = 0,
                       kOperWrite
                      };

  enum ControlMethod  {kMethodGetBOM = 0,
                       kMethodDoListParSeq,
                       kMethodDoListParSeqTmp,
                       kMethodSendDLM,
                       kMethodFlibRead,
                       kMethodFlibWrite
                      };

  enum ControlStatus  {kStatusOK = 0,
                       kStatusNotYet,
                       kStatusFail,
                       kStatusFailRMC,
                       kStatusFailTOutSoft,
                       kStatusFailTOutHard,
                       kStatusFailNack
                      };
  
  enum ControlPorts   {kPortControl   = 9750,
                       kPortDirectory = 9751
                      };
    

} // end namespace CbmNet

#endif
