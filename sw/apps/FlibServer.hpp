// server class to send dcm and dlm from zmq

#include <vector>
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <sys/eventfd.h>

#include <flib.h>
#include "zmq.hpp"
#include "global.hpp"

class FlibServer {
  
  flib _flib;
  zmq::context_t& _zmq_context;
  zmq::socket_t _driver_req;
  zmq::socket_t _driver_res;
  boost::thread _driver_thread;
  int _stop_fd; 
  std::vector<zmq_pollitem_t> _poll_items;

  enum driver_state_t {
    DriverStateStopped,
    DriverStateRunning,
    DriverStateStopping
  };
  driver_state_t _driver_state;
  

public:
  

  FlibServer(zmq::context_t&  context)
    :       _flib(0),
            _zmq_context(context),
            _driver_req(context, ZMQ_PULL),
            _driver_res(context, ZMQ_PUSH),
            _stop_fd(-1),
            _driver_state(DriverStateStopped)
  { }
  
  ~FlibServer()
  {
    Stop();
  }

  void Bind()
  {
    _driver_req.bind("inproc://CbmNet::ControlServer::DriverReq");
    _driver_res.bind("inproc://CbmNet::ControlServer::DriverRes");
    return;
  }

  bool Start()
  {
    // initialize FLIB
    _flib.add_link(0);
    _flib.link[0]->set_data_rx_sel(cbm_link::pgen);

    // setup fd for stopping driver
    _stop_fd = ::eventfd(0,0);
    if (_stop_fd < 0) {
      return false;
    }
    
    // setup poll list
    _poll_items.clear();
    zmq_pollitem_t zpi;
    zpi.socket  = 0;
    zpi.fd      = _stop_fd;
    zpi.events  = ZMQ_POLLIN;
    zpi.revents = 0;
    _poll_items.push_back(zpi);

    zpi.socket  = _driver_req;
    zpi.fd      = 0;
    _poll_items.push_back(zpi);

    // start driver thread
    _driver_thread =  boost::thread(boost::bind(&FlibServer::Driver, this));
    
    return true;
  }

  void Stop()
  {
    // if driver is already stopped do nothing
    if (_driver_state == DriverStateStopped) return;
    
    // send stop event to driver thread and join thread
    if (_driver_state != DriverStateStopped) {
      uint64_t one(1);
      int irc = ::write(_stop_fd, &one, sizeof(one));
      if (irc != sizeof(one)) {
        throw 1; // TODO
      }
    }
    
    if (_driver_thread.get_id() != boost::thread::id()) {
      if (!_driver_thread.timed_join(boost::posix_time::milliseconds(500))) {
        throw 1; // TODO
      }
    } 
    
    // close wakeup eventfd
    if (_stop_fd > 0) ::close(_stop_fd);
    _stop_fd = -1;
    
    _poll_items.clear();
    std::cout << "driver thread terminated" << std::endl;

    return;
  }

  void Driver() 
  {
    _driver_state = DriverStateRunning;  
    // event loop
    while (_driver_state == DriverStateRunning) {
      // poll for events
      int rc = zmq::poll(_poll_items.data(), _poll_items.size(), -1);
      if (rc==-1 && errno==EINTR) continue;

      for (size_t i=0; i<_poll_items.size(); i++) {
        if (_poll_items[i].revents == 0) continue;
        if (_poll_items[i].fd == _stop_fd) {
          // stop driver
          uint64_t ret;
          ::read(_stop_fd, &ret, sizeof(ret)); // todo
          _driver_state = DriverStateStopped;
        }
        else if (_poll_items[i].socket == _driver_req) {
          // process message
          ProcEvent();
        }
      } 
    }
    return;
  }

  void ProcEvent()
  {
    out.setVerbosity(einhard::DEBUG);

    ctrl_msg cnet_r_msg;
    ctrl_msg cnet_s_msg;
   
    // get messsage
    size_t msg_size = _driver_req.recv(cnet_s_msg.data, sizeof(cnet_s_msg.data));
    if (msg_size > sizeof(cnet_s_msg.data)) {
      cnet_s_msg.words = sizeof(cnet_s_msg.data)/sizeof(cnet_s_msg.data[0]);
      std::cout << "message truncated" << std::endl;
    }
    else {
      cnet_s_msg.words = msg_size/sizeof(cnet_s_msg.data[0]);
    }

    //DEBUG
    for (size_t i = 0; i < cnet_s_msg.words; i++) {
      out.debug() << "msg to send " << std::hex << std::setfill('0') 
                  <<  "0x" << std::setw(4) << cnet_s_msg.data[i];
    }
    
    // receive to flush hw buffers
    if ( _flib.link[0]->rcv_msg(&cnet_r_msg) != -1)  {
      out.warn() << "sprious message dropped";
    }
  
    if ( _flib.link[0]->send_msg(&cnet_s_msg) < 0)  {
      out.error() << "sending message failed";
    }                      
    
    // receive msg
    if ( _flib.link[0]->rcv_msg(&cnet_r_msg) < 0)  {
      out.error() << "receiving message failed";
    }

    //DEBUG
    for (size_t i = 0; i < cnet_r_msg.words; i++) {
      out.debug() << "msg received " << std::hex << std::setfill('0') 
                  <<  "0x" << std::setw(4) << cnet_r_msg.data[i];
    }

    _driver_res.send(cnet_r_msg.data, cnet_r_msg.words*sizeof(cnet_r_msg.data[0]));
    return;
  }

};
