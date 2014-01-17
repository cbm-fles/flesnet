// $Id: cliserver.cpp 17 2013-08-09 21:30:26Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#include <unistd.h>
#include <csignal>

#include "zmq.hpp"

#include "control/libserver/ControlServer.hpp"

using namespace std;
using namespace CbmNet;

// ---------------------------------------------------------------------------

int s_interrupted = 0;
static void s_signal_handler (int signal_value)
{
  s_interrupted = 1;
  std::cout << "exisfdgrting" << std::endl;
}

static void s_catch_signals (void)
{
  struct sigaction action;
  action.sa_handler = s_signal_handler;
  action.sa_flags = 0;
  sigemptyset (&action.sa_mask);
  sigaction (SIGABRT, &action, NULL);
  sigaction (SIGTERM, &action, NULL);
  sigaction (SIGINT, &action, NULL);
}

int main(int argc, const char* argv[]) 
{
  zmq::context_t zmq_context(1);

  //boost::thread thread_cserv;

  ControlServer cntlserv(zmq_context, "dontcare");
  cntlserv.SetDebugFlags(ControlServer::kDbgDumpRpc |
                         ControlServer::kDbgLoopBack);
  cntlserv.Bind();

  cntlserv.Start();
  s_catch_signals();

  while(s_interrupted==0) {
    sleep(1);
  }

  return 0;
}
