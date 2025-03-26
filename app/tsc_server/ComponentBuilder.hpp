// Copyright 2025 Dirk Hutter
#pragma once

#include "cri_channel.hpp"
#include "cri_source.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>

namespace ip = boost::interprocess;

template <typename T_DESC, typename T_DATA> class ComponentBuilder {

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

  std::unique_ptr<cri_source<T_DESC, T_DATA>> m_cri_source_buffer;

  constexpr static size_t data_item_size = sizeof(T_DATA);
  constexpr static size_t desc_item_size = sizeof(T_DESC);
};
