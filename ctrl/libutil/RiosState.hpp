// $Id: RiosState.hpp 20 2013-08-10 16:42:54Z mueller $
//
// Copyright 2006-2011 by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//
// This program is free software; you may redistribute and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 2, or at your option any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for complete details.
// 
// Imported Rev 486/357 1.0 from Retro -> CbmNet
// ---------------------------------------------------------------------------


/*!
  \file
  \version $Id: RiosState.hpp 20 2013-08-10 16:42:54Z mueller $
  \brief   Declaration of class RiosState.
*/

#ifndef included_CbmNet_RiosState
#define included_CbmNet_RiosState 1

#include <ios>

namespace CbmNet {

  class RiosState {
    public:
                    RiosState(std::ios& stream);
                    RiosState(std::ios& stream, const char* form, int prec=-1);
                    ~RiosState();

      void          SetFormat(const char* form, int prec=-1);
      char          Ctype();

    protected:
      std::ios&	    fStream;
      std::ios_base::fmtflags  fOldFlags;
      int	    fOldPrecision;
      char	    fOldFill;
      char	    fCtype;

    // RiosState can't be default constructed, copied or assigned
    private:
                    RiosState();
                    RiosState(const RiosState& rhs);
      RiosState&    operator=(const RiosState& rhs);

  };
  
} // end namespace CbmNet

#include "RiosState.ipp"

#endif
