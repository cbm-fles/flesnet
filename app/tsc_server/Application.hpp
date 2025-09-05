// Copyright 2025 Dirk Hutter, Jan de Cuveland
#pragma once

#include "Channel.hpp"
#include "Monitor.hpp"
#include "Parameters.hpp"
#include "Scheduler.hpp"
#include "StSender.hpp"
#include "cri_device.hpp"
#include "pgen_channel.hpp"
#include <boost/interprocess/interprocess_fwd.hpp>
#include <csignal>
#include <map>
#include <memory>
#include <sys/types.h>
#include <vector>

/// %Application base class.
class Application {
public:
  Application(Parameters const& par, volatile sig_atomic_t* signal_status);

  Application(const Application&) = delete;
  void operator=(const Application&) = delete;

  ~Application();

  void run();

private:
  void handle_completions();
  void provide_subtimeslice(std::vector<Channel::State> const& states,
                            uint64_t start_time,
                            uint64_t duration);

  Parameters const& m_par;
  volatile std::sig_atomic_t* m_signal_status;

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
  std::string m_hostname;

  Scheduler m_tasks;

  std::unique_ptr<StSender> m_st_sender;
};
