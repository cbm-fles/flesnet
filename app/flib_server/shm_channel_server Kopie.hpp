class shm_channel_server {

public:

  shm_channel_server(managed_shared_memory* shm,
                     shm_device* shm_dev,
                     size_t index,
                     std::size_t data_buffer_size_exp,
                     std::size_t desc_buffer_size_exp)
    : m_shm(shm), m_shm_dev(shm_dev)
  {
    // allocate buffers
    m_data_buffer = alloc_buffer(data_buffer_size_exp);
    m_desc_buffer = alloc_buffer(desc_buffer_size_exp);

    // constuct channel exchange object in sharde memory
    std::string channel_name = "shm_channel_" +
      boost::lexical_cast<std::string>(index);
    m_shm_ch = m_shm->construct<shm_cahnnel>(channel_name.c_str())
      (m_shm, m_data_buffer, m_desc_buffer);
  }

  ~shm_channel_server() {
    // TODO
    // destroy channel object and dallocate buffers if it is worth to do
    stop();
  }
  
  run() {
    if (!m_run) { // don't start twice
      m_run = true;
      while (m_run) {
        scoped_lock<interprocess_mutex> lock(m_shm_ch->m_mutex);
        
        // check nothing is todo everytime before sleeping
        if (!m_shm_ch->m_req_ptr && !m_shm_ch->m_req_offset) {
          m_shm_ch->m_cond_req.wait(lock);
        }
        
        if (m_shm_ch->m_req_ptr) {
          m_data_ptr_cached = m_shm_ch->m_data_read_ptr;
          m_desc_ptr_cached = m_shm_ch->m_desc_read_ptr;      
          m_shm_ch->m_req_ptr = false; // reset req before releasing lock ensures not to miss last req
          lock.unlock();
          // do fancy HW stuff here
          lock.lock();      
        }
        
        if (m_shm_ch->m_req_offset) {
          m_shm_ch->m_req_offset = false;
          lock.unlock();
          // fetch information from HW
          // set m_data_offset_cached
          lock.lock(); 
          m_shm_ch->m_data_offset = m_data_offset_cached;
          m_shm_ch->m_desc_offset = m_desc_offset_cached;
        }
      }
    }
  }

  stop() {
    m_run = false;
  }
  
private:

  void* alloc_buffer(size_t size_exp) {
    return m_shm->allocate_aligned(UINT64_C(1) << size_exp, sysconf(_SC_PAGESIZE));
  }
  
  
  managed_shared_memory* m_shm;
  shm_device* m_shm_dev;

  void* m_data_buffer;
  void* m_desc_buffer;
  shm_channel* m_shm_ch;
  
  bool m_run = false;

  uint64_t m_data_prt_cached = 0;
  uint64_t m_desc_prt_cached = 0;
  uint64_t m_data_offset_cached = 0;
  uint64_t m_desc_offset_cached = 0;
  
};
