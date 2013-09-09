// server class to send dcm and dlm from zmq

#include <vector>
#include <iostream>
#include <cstdint>
#include <sys/eventfd.h>

#include <libflib.h>
#include "zmq.hpp"

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
    zmq::message_t zmq_r_msg;

    ctrl_msg cnet_r_msg;
    ctrl_msg cnet_s_msg;
   
    // get incoming messsage
    _driver_req.recv(&zmq_r_msg);
    
    cnet_s_msg.words = zmq_r_msg.size()/sizeof(uint16_t);
    for (size_t i = 0; i< cnet_s_msg.words; i++) {
      cnet_s_msg.data[i] = ((uint16_t*)zmq_r_msg.data())[i];
    }

    //DEBUG
    for (size_t i = 0; i < cnet_s_msg.words; i++) {
      printf("msg to send: 0x%04x\n", ((uint16_t*)zmq_r_msg.data())[i]);
    }

      // receive to flush hw buffers
    if ( _flib.link[0]->rcv_msg(&cnet_r_msg) == 0)  {
      printf("spurious message dropped\n");
    }
  
    if ( _flib.link[0]->send_msg(&cnet_s_msg) < 0)  {
      printf("sending message failed\n");
    }                      
    
    // receive msg
    if ( _flib.link[0]->rcv_msg(&cnet_r_msg) < 0)  {
      printf("receiving message failed\n");
    }

    //DEBUG
    for (size_t i = 0; i < cnet_r_msg.words; i++) {
      printf("msg received: 0x%04x\n", cnet_r_msg.data[i]);
    }

    _driver_res.send(cnet_r_msg.data, cnet_r_msg.words*sizeof(uint16_t));
    return;
  }
  

};
