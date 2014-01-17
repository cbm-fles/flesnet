// $Id: RosFill.hpp 20 2013-08-10 16:42:54Z mueller $
//
// Copyright 2000-2011 by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
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
// Imported Rev 486/364 1.1 from Retro -> CbmNet
// ---------------------------------------------------------------------------

/*!
  \file
  \version $Id: RosFill.hpp 20 2013-08-10 16:42:54Z mueller $
  \brief   Declaration of class RosFill .
*/

#ifndef included_CbmNet_RosFill
#define included_CbmNet_RosFill 1

#include <ostream>
#include <string>

namespace CbmNet {
  
  class RosFill {
    public: 
		    RosFill(int count=0, char fill=' ');

      int           Count() const;
      char          Fill() const;

  private:
      int           fCount;		    //!< blank count
      char	    fFill;		    //!< fill character

  };

  std::ostream&	    operator<<(std::ostream& os, const RosFill& obj);
  std::string& 	    operator<<(std::string&  os, const RosFill& obj);

} // end namespace CbmNet

#include "RosFill.ipp"

#endif
