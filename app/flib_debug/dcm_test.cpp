#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <limits.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <csignal>
#include <chrono>

#include <flib.h>

#include "mc_functions.h"

using namespace std;
using namespace flib;

flib_device* MyFlib = NULL;


int main(int argc, char *argv[])
{

  size_t iter = 0;
  if ( argc == 2 ) {
    iter = atoi(argv[1]);
  } else {
    cout << "provide iterations as argument" << endl;
    return -1;
  }  

  MyFlib = new flib_device(0);
 
  //MyFlib->m_link[0]->set_data_rx_sel(flib_link::link);
 
  ctrl_msg s_msg;
  ctrl_msg r_msg;
  // ctrl_msg extra_msg;

  size_t words;
  uint16_t offset;

  // receive to flush hw buffers
  if ( MyFlib->m_link[0]->recv_dcm(&r_msg) >= 0)  {
    printf("Error Message in cue\n");
    return -1;
  }
  
  srand (time(NULL));

  size_t i = 0;
  while (i < iter) {

    words = rand() % 29 + 4; // range 4 to 32
    //words = 23;
    offset = (uint16_t)rand();

//    cout << "iter " << i
//         << " words " << words 
//         << " offset " << hex << offset << dec << endl;
    
    // fill message
    for (size_t i = 0; i < words; i++) {
      s_msg.data[i] = offset + i;
    }
    s_msg.words = words;

    // send msg
    if ( MyFlib->m_link[0]->send_dcm(&s_msg) < 0)  {
      printf("sending failed\n");
      return -1;
    }                      

    //    usleep(1);
    
//    // receive msg
//    if ( MyFlib->m_link[0]->recv_dcm(&r_msg) < 0)  {
//      printf("error receiving\n");
//      return -1;
//    }
 
    
    // receive msg
    int ret = -1;
    int timeout = 900;
    auto timeout_tp = std::chrono::high_resolution_clock::now()
      + std::chrono::microseconds(timeout);
    // poll till msg is available, error occures or timeout is reached
    while ( ret == -1 &&
            std::chrono::high_resolution_clock::now() < timeout_tp) {
      ret = MyFlib->m_link[0]->recv_dcm(&r_msg);
    }
    if (ret == -2) {
      cout<< "received message with illegal size" << endl;
      return -1;
    } else if ( ret == -1)  {
      cout << "timeout receiving message" << endl;
      return -1;
    } 

    // check msg
   if (s_msg.words != r_msg.words) {
      cout << "iter " << i 
           << " words missmatch: send " << s_msg.words 
           << " recv " << r_msg.words << endl;
    } else {
      for (size_t j = 0; j < r_msg.words; j++) {
        if (s_msg.data[j] != r_msg.data[j]) {
          cout << "iter " << i 
               << " word " << j
               << " words in msg " <<  r_msg.words
               << hex
               << " data missmatch: send 0x" << s_msg.data[j] 
               << " recv 0x" << r_msg.data[j] 
               << dec << endl;
        }
      }
    }

 
//    // extra receive
//    for (size_t j = 0; j < r_msg.words/2 + 1; ++j) {
//      ((uint32_t*)extra_msg.data)[j] = MyFlib->m_link[0]->get_rfgtx()->get_reg(RORC_MEM_BASE_CTRL_RX+j);
//    }
//    extra_msg.words = words;
//    
//    for (size_t j = 0; j < extra_msg.words; j++) {
//      if (s_msg.data[j] != extra_msg.data[j]) {
//        cout << "iter " << i << hex
//             << " extra data missmatch: send 0x" << s_msg.data[j] 
//             << " recv 0x" << extra_msg.data[j] 
//             << dec << endl;
//      }
//    }
    ++i;
  }

  if (MyFlib) delete MyFlib;
  
  return 0;
}
