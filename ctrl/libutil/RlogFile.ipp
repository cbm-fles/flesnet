// $Id: RlogFile.ipp 20 2013-08-10 16:42:54Z mueller $
//
// Copyright 2011-2013 by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
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
// Imported Rev 492/492 2.1 from Retro -> CbmNet
// ---------------------------------------------------------------------------

/*!
  \file
  \version $Id: RlogFile.ipp 20 2013-08-10 16:42:54Z mueller $
  \brief   Implemenation (inline) of RlogFile.
*/

// all method definitions in namespace CbmNet
namespace CbmNet {

//------------------------------------------+-----------------------------------
//! FIXME_docs

inline bool RlogFile::IsNew() const
{
  return fNew;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

inline const std::string& RlogFile::Name() const
{
  return fName;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

inline std::ostream& RlogFile::Stream()
{
  return fpExtStream ? *fpExtStream : fIntStream;
}

} // end namespace CbmNet
