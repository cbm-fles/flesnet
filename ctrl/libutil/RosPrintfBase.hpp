// $Id: RosPrintfBase.hpp 20 2013-08-10 16:42:54Z mueller $
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
// Imported Rev 486/364 1.1 from Retro -> CbmNet
// ---------------------------------------------------------------------------

/*!
  \file
  \version $Id: RosPrintfBase.hpp 20 2013-08-10 16:42:54Z mueller $
  \brief   Declaration of RosPrintfBase class .
*/

#ifndef included_CbmNet_RosPrintfBase
#define included_CbmNet_RosPrintfBase 1

#include <ostream>
#include <string>

namespace CbmNet {

  class RosPrintfBase {
    public:
                    RosPrintfBase(const char* form, int width, int prec);
      virtual       ~RosPrintfBase();

      virtual void  ToStream(std::ostream& os) const = 0;

    protected:
      const char*   fForm;		    //!< format string
      int	    fWidth;		    //!< field width
      int	    fPrec;                  //!< field precision
  };

  std::ostream& operator<<(std::ostream& os, const RosPrintfBase& obj);
  std::string&  operator<<(std::string& os,  const RosPrintfBase& obj);

} // end namespace CbmNet

#include "RosPrintfBase.ipp"

#endif
