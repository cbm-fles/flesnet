class shm_channel {

public:

  shm_channel(managed_shared_memory* shm,
             void* data_buffer,
             void* desc_buffer) {

    set_buffer_handles(shm, data_buffer, desc_buffer);
  }
  
  ~shm_channel() {}
  
  void* data_buffer_ptr(managed_shared_memory* shm) const {
    return shm->get_address_from_handle(data_buffer_handle);
  }

  void* desc_buffer_ptr(managed_shared_memory* shm) const {
    return shm->get_address_from_handle(data_buffer_handle);
  }

  // getter / setter
  bool req_ptr(interprocess_mutex& lock) {
    assert(lock);
    return m_req_ptr;
  }
  
  bool req_offset(interprocess_mutex& lock) {
    assert(lock);
    return m_req_offset;
  }

  void set_req_ptr(interprocess_mutex& lock, bool req) {
    assert(lock);
    m_req_ptr = req;
  }

  void set_req_offset(interprocess_mutex& lock, bool req) {
    assert(lock);
    m_req_offset = req;
  }
  
  void read_ptrs(interprocess_mutex& lock,
                uint64_t& data_read_ptr,
                uint64_t& desc_read_ptr) {
    assert(lock);
    data_read_ptr = m_data_read_ptr;
    desc_read_ptr = m_desc_read_ptr;
    return;
  }

  void offsets(interprocess_mutex& lock,
               uint64_t& data_offset,
               uint64_t& desc_offset) {
    assert(lock);
    data_offset = m_data_offset;
    desc_offset = m_desc_offset;
    return;
  }

  void set_offsets(interprocess_mutex& lock,
                   const uint64_t data_offset,
                   const uint64_t desc_offset) {
    assert(lock);
    m_data_offset = data_offset;
    m_desc_offset = desc_offset;
    return;
  }
  
  void set_read_ptrs(interprocess_mutex& lock,
                    const uint64_t data_read_ptr,
                    const uint64_t desc_read_ptr) {
    assert(lock);
    m_data_read_ptr = data_read_ptr;
    m_desc_read_ptr = desc_read_ptr;
    return;
  }
  

private:

  void set_buffer_handles(managed_shared_memory* shm,
                          void* data_buffer,
                          void* desc_buffer) {
    m_data_buffer_handle = shm->get_handle_from_address(data_buffer);
    m_desc_buffer_handle = shm->get_handle_from_address(desc_buffer);
  }
  
  managed_shared_memory::handle m_data_buffer_handle;
  managed_shared_memory::handle m_desc_buffer_handle;

  bool m_req_ptr = false;
  bool m_req_offset = false;
  
  uint64_t m_data_read_prt = 0; // TODO initialize from hw
  uint64_t m_desc_read_prt = 0;  
  uint64_t m_data_offset = 0;
  uint64_t m_desc_offset = 0;
  
};
