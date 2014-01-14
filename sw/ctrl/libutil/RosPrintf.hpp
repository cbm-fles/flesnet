// $Id: RosPrintf.hpp 20 2013-08-10 16:42:54Z mueller $
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
  \version $Id: RosPrintf.hpp 20 2013-08-10 16:42:54Z mueller $
  \brief   Declaration of RosPrintf functions.

  For a detailed description of the usage of the \c RosPrintf system
  look into \ref using_rosprintf.
*/

#ifndef included_CbmNet_RosPrintf
#define included_CbmNet_RosPrintf 1

#include "RosPrintfS.hpp"

namespace CbmNet {
  
  RosPrintfS<char>   RosPrintf(char value, const char* form=0, 
                               int width=0, int prec=0);

  RosPrintfS<int>    RosPrintf(signed char value, const char* form=0, 
                               int width=0, int prec=0);
  RosPrintfS<unsigned int> RosPrintf(unsigned char value, const char* form=0, 
                                     int width=0, int prec=0);

  RosPrintfS<int>    RosPrintf(short value, const char* form=0,
                               int width=0, int prec=0);
  RosPrintfS<unsigned int> RosPrintf(unsigned short value, const char* form=0,
                                     int width=0, int prec=0);

  RosPrintfS<int>    RosPrintf(int value, const char* form=0, 
                               int width=0, int prec=0);
  RosPrintfS<unsigned int> RosPrintf(unsigned int value, const char* form=0,
                                     int width=0, int prec=0);

  RosPrintfS<long>    RosPrintf(long value, const char* form=0, 
                                int width=0, int prec=0);
  RosPrintfS<unsigned long> RosPrintf(unsigned long value, const char* form=0,
                                      int width=0, int prec=0);

  RosPrintfS<double>   RosPrintf(double value, const char* form=0, 
                                 int width=0, int prec=0);

  RosPrintfS<const char*> RosPrintf(const char* value, const char* form=0, 
                                    int width=0, int prec=0);

  RosPrintfS<const void*> RosPrintf(const void* value, const char* form=0, 
                                    int width=0, int prec=0);

} // end namespace CbmNet

// implementation is all inline
#include "RosPrintf.ipp"

#endif
