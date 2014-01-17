// $Id: RlogFile.hpp 20 2013-08-10 16:42:54Z mueller $
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
  \version $Id: RlogFile.hpp 20 2013-08-10 16:42:54Z mueller $
  \brief   Declaration of class RlogFile.
*/

#ifndef included_CbmNet_RlogFile
#define included_CbmNet_RlogFile 1

#include <string>
#include <ostream>
#include <fstream>

#include "boost/utility.hpp"
#include "boost/thread/mutex.hpp"

namespace CbmNet {

  class RlogMsg;                            // forw decl to avoid circular incl

  class RlogFile : private boost::noncopyable {
    public:
                    RlogFile();
      explicit      RlogFile(std::ostream* os, const std::string& name = "");
                   ~RlogFile();

      bool          IsNew() const;
      bool          Open(std::string name);
      void          Close();
      void          UseStream(std::ostream* os, const std::string& name = "");
      const std::string&  Name() const;

      void          Write(const std::string& str, char tag = 0);

      void          Dump(std::ostream& os, int ind=0, const char* text=0) const;

      // provide boost Lockable interface
      void          lock();
      void          unlock();

      RlogFile&     operator<<(const RlogMsg& lmsg);

    protected:
      std::ostream& Stream();
      void          ClearTime();

    protected:
      std::ostream* fpExtStream;            //!< pointer to external stream
      std::ofstream fIntStream;             //!< internal stream
      bool          fNew;                   //!< true if never opened or used
      std::string   fName;                  //!< log file name
      int           fTagYear;               //!< year of last time tag
      int           fTagMonth;              //!< month of last time tag
      int           fTagDay;                //!< day of last time tag
      boost::mutex  fMutex;                 //!< mutex to lock file
  };
  
} // end namespace CbmNet

#include "RlogFile.ipp"

#endif
