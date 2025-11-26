/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Authors: Dirk Hutter, Jan de Cuveland */
#pragma once

#include "Channel.hpp"
#include "Monitor.hpp"
#include "Parameters.hpp"
#include "Scheduler.hpp"
#include "StSender.hpp"
#include "SubTimeslice.hpp"
#include "cri_device.hpp"
#include "pgen_channel.hpp"
#include <boost/interprocess/interprocess_fwd.hpp>
#include <csignal>
#include <map>
#include <memory>
#include <sys/types.h>
#include <vector>

/// %StBuilder base class.
class StBuilder {
public:
  StBuilder(volatile sig_atomic_t* signal_status,
            cbm::Monitor* monitor,
            SenderInfo sender_info,
            StSender& st_sender,
            bool device_autodetect,
            pci_addr device_address,
            std::string shm_id,
            uint32_t pgen_channels,
            int64_t pgen_microslice_duration_ns,
            size_t pgen_microslice_size,
            uint32_t pgen_flags,
            int64_t timeslice_duration_ns,
            int64_t timeout_ns,
            size_t data_buffer_size,
            size_t desc_buffer_size,
            int64_t overlap_before_ns,
            int64_t overlap_after_ns);

  StBuilder(const StBuilder&) = delete;
  void operator=(const StBuilder&) = delete;

  ~StBuilder();

  void run();

private:
  void handle_completions();
  void provide_subtimeslice(std::vector<Channel::State> const& states,
                            uint64_t start_time,
                            uint64_t duration);

  volatile std::sig_atomic_t* m_signal_status;
  std::string m_shm_id;

  // Timing configuration
  int64_t m_timeslice_duration_ns;
  int64_t m_timeout_ns;
  int64_t m_overlap_before_ns;
  int64_t m_overlap_after_ns;

  std::vector<std::unique_ptr<cri::cri_device>> m_cris;
  std::vector<cri::cri_channel*> m_cri_channels;
  std::vector<std::unique_ptr<cri::pgen_channel>> m_pgen_channels;
  std::unique_ptr<boost::interprocess::managed_shared_memory> m_shm;
  std::vector<std::unique_ptr<Channel>> m_channels;

  std::map<uint64_t, bool> m_completed;

  size_t m_timeslice_count = 0;  ///< total number of processed timeslices
  size_t m_component_count = 0;  ///< total number of processed components
  size_t m_microslice_count = 0; ///< total number of processed microslices
  size_t m_data_bytes = 0;       ///< total number of processed content bytes
  size_t m_timeslice_incomplete_count = 0; ///< number of incomplete timeslices

  void report_status();

  std::unique_ptr<cbm::Monitor> m_monitor;
  SenderInfo m_sender_info;

  Scheduler m_tasks;

  StSender& m_st_sender;
};
