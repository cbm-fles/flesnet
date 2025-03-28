// Copyright 2025 Dirk Hutter
#pragma once

#include "cri_channel.hpp"
#include "cri_source.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>

namespace ip = boost::interprocess;

class ComponentBuilder {

public:
  ComponentBuilder(ip::managed_shared_memory* shm,
                   cri::cri_channel* cri_channel,
                   size_t data_buffer_size_exp,
                   size_t desc_buffer_size_exp);

  ~ComponentBuilder();

  void proceed();

private:
  void* alloc_buffer(size_t size_exp, size_t item_size);

  ip::managed_shared_memory* m_shm;
  cri::cri_channel* m_cri_channel;

  std::unique_ptr<cri_source> m_cri_source_buffer;
};
