// $Id: cliclient.cpp 24 2013-08-13 18:12:56Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <cctype>
#include <sstream>

#include "libutil/Util.hpp"
#include "libutil/ConvertInt.hpp"
#include "libutil/RosPrintf.hpp"
#include "control/libclient/ControlClient.hpp"
#include "control/libclient/ListSeq.hpp"

using namespace std;
using namespace CbmNet;

// ---------------------------------------------------------------------------

void quit(const string& str) 
{
  cerr << str << endl;
  exit(EXIT_FAILURE);  
}

// ---------------------------------------------------------------------------

template <class T>
void convert(const string& str, T& val, int base=0)
{
  RerrMsg emsg;
  if (!ConvertInt(str, val, emsg, base))  quit(emsg);
  return;
}

// ---------------------------------------------------------------------------

void usage()
{
  cout << "usage: cliclient dserv_path [command .. values] ..." << endl;
  cout << "dserv_path: hostname:port" << endl;
  cout << "  port = 9750 + flib_link_index" << endl;
  cout << "commands:" << endl;
  cout << "  read nodeid addr" << endl;
  cout << "  write nodeid addr value" << endl;
  cout << "  readseq nodeid addr addrinc nval" << endl;
  cout << "  writeseq nodeid addr addrinc value ..." << endl;
  cout << "  listseq nodeid [-r addr] [-w addr value] ..." << endl;
  cout << "  listpar [-r nodeid addr [-w nodeid addr value] ..." << endl;
  cout << "  listparseq [-n nodeid [[-r addr] [-w addr value] ... ]..." << endl;
  cout << "  bom nodeid" << endl;
  cout << "  dlm nodeid dnum" << endl;
  cout << "  fread nodeid addr" << endl;
  cout << "  fwirte nodeid addr data" << endl;
  return;
}

// ---------------------------------------------------------------------------

int main(int argc, const char* argv[])
{
  if (argc < 2) quit ("Missing dserv_path. Check usage with -h");
  string arg1(argv[1]);
  if (arg1 == "-?" || arg1 == "-h" || arg1 == "-help") {
    usage();
    return 0;      
  }

  ControlClient conn;
  ostringstream dpath;
  // FIXME_code: connect now to control server, will later have two connects!!
  dpath << "tcp://" << arg1;
  conn.Connect(dpath.str());

  for (int iarg=2; iarg<argc; ) {
    string cmd(argv[iarg++]);
    if        (cmd == "read") {             // -- read -----------------------
      if (iarg+2 > argc) quit("Missing arguments after 'read'");
      uint32_t nodeid;
      uint32_t addr;
      convert(argv[iarg++], nodeid);
      convert(argv[iarg++], addr);
      uint32_t value; 
      int rc = conn.Read(nodeid, addr, value);
      cout << "rc=" << RosPrintf(rc,"d",4)
           << "  addr=" << RosPrintf(addr,"x0",8)
           << "  value=" << RosPrintf(value,"x0",8)
           << " (" << RosPrintf(value,"d",10) << ")";
      if (rc<0) cout << "  status=" << Status2String(-rc);
      cout << endl;

    } else if (cmd == "write") {            // -- write ----------------------
      if (iarg+3 > argc) quit("Missing arguments after 'write'");
      uint32_t nodeid;
      uint32_t addr;
      uint32_t value;
      convert(argv[iarg++], nodeid);
      convert(argv[iarg++], addr);
      convert(argv[iarg++], value);
      int rc = conn.Write(nodeid, addr, value);
      cout << "rc=" << RosPrintf(rc,"d",4)
           << "  addr=" << RosPrintf(addr,"x0",8)
           << "  value=" << RosPrintf(value,"x0",8)
           << " (" << RosPrintf(value,"d",10) << ")";
      if (rc<0) cout << "  status=" << Status2String(-rc);
      cout << endl;

    } else if (cmd == "readseq") {          // -- readseq --------------------
      if (iarg+4 > argc) quit("Missing arguments after 'readseq'");
      uint32_t nodeid;
      uint32_t addr;
      uint32_t addrinc;
      uint32_t nval;
      convert(argv[iarg++], nodeid);
      convert(argv[iarg++], addr);
      convert(argv[iarg++], addrinc);
      convert(argv[iarg++], nval);

      vector<uint32_t> values;
      uint32_t status;
      int rc = conn.ReadSeq(nodeid, addr, values, nval, status, addrinc);

      cout << "rc=" << RosPrintf(rc,"d",4);
      if (rc<0) cout << "  status=" << Status2String(-rc);
      cout << endl;
      for (size_t i=0; i<nval; i++) {
        cout << "  " << RosPrintf(i,"d",4)
             << " : " << RosPrintf(values[i],"x0",8) 
             << " (" << RosPrintf(values[i],"d",10) << ")"
             << " : " << ( (rc<0 || int(i)>=rc) ? "FAIL" : "OK")
             << endl;
      }

    } else if (cmd == "writeseq") {         // -- writeseq -------------------
      if (iarg+4 > argc) quit("Missing arguments after 'writeseq'");
      uint32_t nodeid;
      uint32_t addr;
      uint32_t addrinc;
      convert(argv[iarg++], nodeid);
      convert(argv[iarg++], addr);
      convert(argv[iarg++], addrinc);
      vector<uint32_t> values;
      while (iarg < argc) {
        if (argv[iarg][0]==0 || !isdigit(argv[iarg][0])) break;
        uint32_t value;
        convert(argv[iarg++], value);
        values.push_back(value);
      }

      uint32_t status;
      int rc = conn.WriteSeq(nodeid, addr, values, status, addrinc);

      cout << "rc=" << RosPrintf(rc,"d",4);
      if (rc<0) cout << "  status=" << Status2String(-rc);
      cout << endl;

    } else if (cmd == "listseq") {          // -- listseq --------------------
      if (iarg+1 > argc) quit("Missing arguments after 'listseq'");
      uint32_t nodeid;
      convert(argv[iarg++], nodeid);
      ListSeq list;
      while (iarg<argc) {
        string opt(argv[iarg]);
        if (opt.substr(0,1) != "-") break;
        iarg++;
        
        if (opt == "-r") {
          if (iarg+1 > argc) quit("Missing arguments after '-r'");
          uint32_t addr;
          convert(argv[iarg++], addr);
          list.AddRead(addr);
        } else if (opt == "-w") {
          if (iarg+2 > argc) quit("Missing arguments after '-w'");
          uint32_t addr;
          uint32_t value;
          convert(argv[iarg++], addr);
          convert(argv[iarg++], value);
          list.AddWrite(addr, value);
        } else {
          quit("unexpected option '" + opt + "' after listseq");
        }
      }
      int rc = conn.DoListSeq(nodeid, list);
      cout << "rc=" << RosPrintf(rc,"d",4);
      if (rc<0) cout << "  status=" << Status2String(-rc);
      cout << endl;
      list.Dump(cout);
      
    } else if (cmd == "listpar") {          // -- listpar --------------------
      ListPar list;
      while (iarg<argc) {
        string opt(argv[iarg]);
        if (opt.substr(0,1) != "-") break;
        iarg++;
        
        if (opt == "-r") {
          if (iarg+2 > argc) quit("Missing arguments after '-r'");
          uint32_t nodeid;
          uint32_t addr;
          convert(argv[iarg++], nodeid);
          convert(argv[iarg++], addr);
          list.AddRead(nodeid, addr);
        } else if (opt == "-w") {
          if (iarg+3 > argc) quit("Missing arguments after '-w'");
          uint32_t nodeid;
          uint32_t addr;
          uint32_t value;
          convert(argv[iarg++], nodeid);
          convert(argv[iarg++], addr);
          convert(argv[iarg++], value);
          list.AddWrite(nodeid, addr, value);
        } else {
          quit("unexpected option '" + opt + "' after listpar");
        }
      }
      int rc = conn.DoListPar(list);
      cout << "rc=" << RosPrintf(rc,"d",4);
      if (rc<0) cout << "  status=" << Status2String(-rc);
      cout << endl;
      list.Dump(cout);

    } else if (cmd == "listparseq") {       // -- listparseq -----------------
      vector<ListSeq> listvec;
      while (iarg<argc) {
        string opt(argv[iarg]);
        if (opt.substr(0,1) != "-") break;
        iarg++;

        if (opt == "-n") {
          if (iarg+1 > argc) quit("Missing arguments after '-n'");
          uint32_t nodeid;
          convert(argv[iarg++], nodeid);
          ListSeq list;
          list.SetNodeid(nodeid);
          
          while (iarg<argc) {
            string opt1(argv[iarg]);
            if (opt1 == "-n" || opt1.substr(0,1) != "-") break;
            iarg++;
            if (opt1 == "-r") {
              if (iarg+1 > argc) quit("Missing arguments after '-r'");
              uint32_t addr;
              convert(argv[iarg++], addr);
              list.AddRead(addr);
            } else if (opt1 == "-w") {
              if (iarg+2 > argc) quit("Missing arguments after '-w'");
              uint32_t addr;
              uint32_t value;
              convert(argv[iarg++], addr);
              convert(argv[iarg++], value);
              list.AddWrite(addr, value);
            } else {
              quit("unexpected option '" + opt + "' after listparseq");
            }
          }
          listvec.push_back(list);

        } else {
          if (opt.substr(0,1) != "-") break;
          quit("unexpected option '" + opt + "' after listparseq");
        }
      }
      int rc = conn.DoListParSeq(listvec);
      cout << "rc=" << RosPrintf(rc,"d",4);
      if (rc<0) cout << "  status=" << Status2String(-rc);
      cout << endl;
      for (size_t i=0; i<listvec.size(); i++) {
        cout << RosPrintf(i,"d",4) << " :" << endl;
        listvec[i].Dump(cout);
      }

    } else if (cmd == "bom") {              // -- bom ------------------------
      if (iarg+1 > argc) quit("Missing arguments after 'bom'");
      uint32_t nodeid;
      convert(argv[iarg++], nodeid);
      uint32_t bom = conn.ServerBom(nodeid);
      cout << "server bom=" << RosPrintf(bom,"x0",4) << endl;

    } else if (cmd == "dlm") {              // -- dlm ------------------------
      if (iarg+2 > argc) quit("Missing arguments after 'dlm'");
      uint32_t nodeid;
      uint32_t dnum;
      convert(argv[iarg++], nodeid);
      convert(argv[iarg++], dnum);
      int rc = conn.SendDLM(nodeid, dnum);
      if (rc<0) cout << "  status=" << Status2String(-rc) << endl;

    } else if (cmd == "fread") {              // -- flib read ------------------------
      if (iarg+2 > argc) quit("Missing arguments after 'fread'");
      
      uint32_t nodeid;
      uint32_t addr;
      uint32_t value;
      convert(argv[iarg++], nodeid);
      convert(argv[iarg++], addr);

      int rc = conn.FlibRead(nodeid, addr, value);
      
      cout << "rc=" << RosPrintf(rc,"d",4)
           << "  addr=" << RosPrintf(addr,"x0",8)
           << "  value=" << RosPrintf(value,"x0",8)
           << " (" << RosPrintf(value,"d",10) << ")";
 
      if (rc<0) cout << "  status=" << Status2String(-rc); 
      cout << endl;

    } else if (cmd == "fwrite") {              // -- flib write ------------------------
      if (iarg+3 > argc) quit("Missing arguments after 'fwrite'");
      
      uint32_t nodeid;
      uint32_t addr;
      uint32_t value;
      convert(argv[iarg++], nodeid);
      convert(argv[iarg++], addr);
      convert(argv[iarg++], value);

      int rc = conn.FlibWrite(nodeid, addr, value);
      
      cout << "rc=" << RosPrintf(rc,"d",4)
           << "  addr=" << RosPrintf(addr,"x0",8)
           << "  value=" << RosPrintf(value,"x0",8)
           << " (" << RosPrintf(value,"d",10) << ")";
 
      if (rc<0) cout << "  status=" << Status2String(-rc); 
      cout << endl;

    } else {                                // -- ... unknown command ... ----
      quit(string("unknown command '") + cmd + "'. Check with usage with -h");
    }
  }
  return 0;
}
