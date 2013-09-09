#include <unistd.h>
#include <boost/thread.hpp>
#include "../../../usbdaq-test/cbmnet/zmq.hpp"
#include "FlibServer.hpp"
#include "../../../usbdaq-test/cbmnet/control/libserver/ControlServer.hpp"

#include <iostream>
#include <csignal>

int s_interrupted = 0;
static void s_signal_handler (int signal_value)
{
  s_interrupted = 1;
  printf("exiting\n");
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
  s_catch_signals();

  zmq::context_t zmq_context(1);

  FlibServer flibserver(zmq_context);
  flibserver.Bind();
  flibserver.Start();
  
  CbmNet::ControlServer cntlserv(zmq_context);
  cntlserv.SetDebugFlags(CbmNet::ControlServer::kDbgDumpRpc);
  cntlserv.Bind();
  cntlserv.ConnectDriver();
  cntlserv.Start();

    while(s_interrupted==0) {
      ::sleep(1);
   }

    std::cout << "exit" << std::endl;
    
  return 0;
}
