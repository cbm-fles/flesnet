


// create shared memory

// create shm_device object
// shared memory vector of handles to shm_channel
// reimpliment push_back to push pointer to handle
// or only increment link counter

// loop over active links and create shm_channel server
// (server will fill shm_device structure)


class shm_device_server {

public:

  shm_device_server(size_t data_buffer_size_exp,
                    size_t desc_buffer_size_exp) {

    size_t num_links = 4; // TODO
    
    // create a big enough shared memory segment
    size_t shm_size =
      (UINT64_C(1) << data_buffer_size_exp +
       UINT64_C(1) << desc_buffer_size_exp +
       sizeof(shm_channel)) * num_links +
      sizeof(shm_device) + 1000;
    shared_memory_object::remove("flib_shared_memory");
    managed_shared_memory m_shm(create_only, "flib_shared_memory",
                                shm_size);
    
    // constuct device exchange object in sharde memory
    std::string device_name = "shm_device";
    m_shm_dev = m_shm.construct<shm_device>(device_name.c_str()) ();

    // loop over flib links and create channels
    for (size_t i = 0; i < num_links, ++i) {
      m_shm_ch_vec.push_back(std::unique_ptr<shm_channel_server>(
         new shm_channel_server(data_buffer_size_exp)));
      m_shm_dev->inc_num_channels();
    }
    
  }

  ~shm_device_server() {
    shared_memory_object::remove("flib_shared_memory");
  }
  
  run() {
    if (!m_run) { // don't start twice
      m_run = true;
      while (m_run) {
        // claim lock at start-up
        scoped_lock<interprocess_mutex> lock(m_shm_dev->mutex());
        
        // check nothing is todo everytime before sleeping
        // INFO: need to loop over all individual requests,
        // alternative would be global request cue.
        bool pending_req = false;
        for (const shm_channel* shm_ch : m_shm_ch_vec) {
          pending_req |= shm_ch->check_pending_req(lock);
        }
        if (!pending_req) {
          m_shm_dev->cond_req().wait(lock);
        }
        
        // try to handle requests
        for (const shm_channel* shm_ch : m_shm_ch_vec) {
          shm_ch->try_handle_req(lock);
        }
      }
    }
  }

  stop() {
    m_run = false;
  }

  
private:

  managed_shared_memory m_shm;
  shm_device* m_shm_dev;
  std::vector<std::unique_ptr<shm_channel_server>> m_shm_ch_vec;

  bool m_run = false;

};
