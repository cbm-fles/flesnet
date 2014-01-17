// $Id: Rexception.hpp 20 2013-08-10 16:42:54Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
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
// Imported Rev 487/487 1.0.1 from Retro -> CbmNet
// ---------------------------------------------------------------------------


/*!
  \file
  \version $Id: Rexception.hpp 20 2013-08-10 16:42:54Z mueller $
  \brief   Declaration of class Rexception.
*/

#ifndef included_CbmNet_Rexception
#define included_CbmNet_Rexception 1

#include <stdexcept>
#include <string>

#include "RerrMsg.hpp"

namespace CbmNet {

  class Rexception : public std::exception {
    public:
                    Rexception();
                    Rexception(const RerrMsg& errmsg);
                    Rexception(const std::string& meth,
                               const std::string& text);
                    Rexception(const std::string& meth, 
                               const std::string& text, int errnum);
                   ~Rexception() throw();

      const char*  what() const throw();
      const RerrMsg& ErrMsg() const;

    protected:
      RerrMsg       fErrmsg;                //!< message object 
  };

} // end namespace CbmNet

#include "Rexception.ipp"

#endif
