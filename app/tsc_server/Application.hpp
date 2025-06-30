// Copyright 2025 Dirk Hutter, Jan de Cuveland
#pragma once

#include "Channel.hpp"
#include "Monitor.hpp"
#include "Parameters.hpp"
#include "RingBuffer.hpp"
#include "Scheduler.hpp"
#include "StSender.hpp"
#include "cri_device.hpp"
#include "pgen_channel.hpp"
#include <boost/interprocess/interprocess_fwd.hpp>
#include <csignal>
#include <memory>
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

  Parameters const& par_;
  volatile std::sig_atomic_t* signal_status_;

  std::vector<std::unique_ptr<cri::cri_device>> cris_;
  std::vector<cri::cri_channel*> cri_channels_;
  std::vector<std::unique_ptr<cri::pgen_channel>> pgen_channels_;
  std::unique_ptr<boost::interprocess::managed_shared_memory> shm_;
  std::vector<std::unique_ptr<Channel>> channels_;

  RingBuffer<uint64_t, true> completions_{20};
  uint64_t acked_ = 0;

  size_t timeslice_count_ = 0;  ///< total number of processed timeslices
  size_t component_count_ = 0;  ///< total number of processed components
  size_t microslice_count_ = 0; ///< total number of processed microslices
  size_t content_bytes_ = 0;    ///< total number of processed content bytes
  size_t total_bytes_ = 0;      ///< total number of processed bytes
  size_t timeslice_incomplete_count_ = 0; ///< number of incomplete timeslices

  void report_status();

  std::unique_ptr<cbm::Monitor> monitor_;
  std::string hostname_;

  Scheduler scheduler_;

  std::unique_ptr<StSender> st_sender_;
};
