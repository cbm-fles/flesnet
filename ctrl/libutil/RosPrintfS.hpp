// $Id: RosPrintfS.hpp 20 2013-08-10 16:42:54Z mueller $
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
// Imported Rev 357/357 1.0 from Retro -> CbmNet
// ---------------------------------------------------------------------------

/*!
  \file
  \version $Id: RosPrintfS.hpp 20 2013-08-10 16:42:54Z mueller $
  \brief   Declaration of class RosPrintfS .
*/

#ifndef included_CbmNet_RosPrintfS
#define included_CbmNet_RosPrintfS 1

#include "RosPrintfBase.hpp"

namespace CbmNet {

  template <class T>
  class RosPrintfS : public RosPrintfBase {
    public:
		    RosPrintfS(T value, const char* form, int width, int prec);

      virtual void  ToStream(std::ostream& os) const;

    protected:
      T             fValue;		    //!< value to be printed
  };

  template <>
  void RosPrintfS<char>::ToStream(std::ostream& os) const;
  template <>
  void RosPrintfS<int>::ToStream(std::ostream& os) const;
  template <>
  void RosPrintfS<const char*>::ToStream(std::ostream& os) const;  
  template <>
  void RosPrintfS<const void*>::ToStream(std::ostream& os) const;  

} // end namespace CbmNet

#endif
