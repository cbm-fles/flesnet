/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */
#pragma once

#include "MicrosliceDescriptor.hpp"
#include "Monitor.hpp"
#include "Scheduler.hpp"
#include "SubTimeslice.hpp"
#include "System.hpp"
#include "TsBuffer.hpp"
#include "ucxutil.hpp"
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <string_view>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>
#include <unistd.h>
#include <unordered_map>

using namespace std::chrono_literals;

// TsBuilder: Receive timeslice announcements from tsmanager, connect to
// senders, receive subtimeslices and aggregate the data to timeslices

enum class StState : uint8_t {
  Allocated = 0,
  Requested = 1,
  Receiving = 2,
  Complete = 3,
  Failed = 4
};

inline std::string to_string(StState state) {
  switch (state) {
  case StState::Allocated:
    return "Allocated";
  case StState::Requested:
    return "Requested";
  case StState::Receiving:
    return "Receiving";
  case StState::Complete:
    return "Complete";
  case StState::Failed:
    return "Failed";
  default:
    return "Unknown";
  }
}

/// A contiguous destination range within the timeslice buffer, received as
/// one tagged message
struct StDataBlock {
  uint64_t offset = 0; ///< absolute offset within the timeslice buffer
  uint64_t size = 0;
};

struct TsHandle {
  TsHandle(std::byte* buffer, StCollection contributions)
      : id(contributions.id), allocated_at_ns(fles::system::current_time_ns()),
        buffer(buffer), sender_ids(std::move(contributions.sender_ids)),
        ms_data_sizes(std::move(contributions.ms_data_sizes)),
        merged_descriptor(std::move(contributions.merged_descriptor)),
        offsets(sender_ids.size()), states(sender_ids.size()),
        state_change_at_ns(sender_ids.size()) {
    // Initialize offsets
    if (!ms_data_sizes.empty()) {
      std::partial_sum(ms_data_sizes.begin(), ms_data_sizes.end() - 1,
                       offsets.begin() + 1);
    }
    // Initialize states
    std::fill(states.begin(), states.end(), StState::Allocated);
    std::fill(state_change_at_ns.begin(), state_change_at_ns.end(),
              allocated_at_ns);
    // Derive the per-contribution transfer block layout: each component
    // arrives as two contiguous tagged messages (microslice descriptors, then
    // content). The sender derives the identical layout from its announced
    // descriptor, of which the merged descriptor contains a copy with
    // absolute offsets, components in sender order.
    blocks.resize(sender_ids.size());
    blocks_remaining.resize(sender_ids.size(), 0);
    std::size_t comp = 0; // running index into merged_descriptor.components
    for (std::size_t ci = 0; ci < sender_ids.size(); ++ci) {
      uint64_t remaining = ms_data_sizes[ci];
      uint64_t pos = offsets[ci];
      while (remaining > 0 && comp < merged_descriptor.components.size()) {
        const auto& c = merged_descriptor.components[comp];
        const uint64_t desc_size =
            c.num_microslices * sizeof(fles::MicrosliceDescriptor);
        if (static_cast<uint64_t>(c.ms_data_offset) != pos ||
            c.ms_data_size < desc_size || c.ms_data_size > remaining) {
          break; // inconsistent component metadata, use fallback below
        }
        blocks[ci].push_back({pos, desc_size});
        blocks[ci].push_back({pos + desc_size, c.ms_data_size - desc_size});
        pos += c.ms_data_size;
        remaining -= c.ms_data_size;
        ++comp;
      }
      if (remaining > 0) {
        // Inconsistent or missing component metadata: fall back to a single
        // full-size block (the transfer will fail via the timeout if the
        // sender disagrees)
        blocks[ci].assign(1, {offsets[ci], ms_data_sizes[ci]});
      } else if (blocks[ci].empty()) {
        // Empty contribution: the sender sends a single zero-size message to
        // keep the protocol synchronous
        blocks[ci].assign(1, {offsets[ci], 0});
      }
      blocks_remaining[ci] = blocks[ci].size();
    }
  }

  // Cannot be moved or copied (pointer to data is used by ucx)
  TsHandle(const TsHandle&) = delete;
  TsHandle& operator=(const TsHandle&) = delete;

  const TsId id;
  const uint64_t allocated_at_ns;
  uint64_t published_at_ns = 0;
  std::byte* const buffer;
  std::vector<std::string> sender_ids; // IDs of the senders
  std::vector<uint64_t> ms_data_sizes; // Sizes of the content data
  StDescriptor merged_descriptor;      // Merged descriptor from manager
  std::vector<uint64_t> offsets;
  std::vector<StState> states;
  std::vector<uint64_t> state_change_at_ns;
  std::vector<std::vector<StDataBlock>> blocks; ///< per contribution
  std::vector<std::size_t> blocks_remaining;    ///< per contribution
  bool is_published = false;
};

class TsBuilder {
public:
  TsBuilder(volatile sig_atomic_t* signal_status,
            TsBuffer& timeslice_buffer,
            std::string_view manager_address,
            int64_t timeout_ns,
            cbm::Monitor* monitor);
  ~TsBuilder();
  TsBuilder(const TsBuilder&) = delete;
  TsBuilder& operator=(const TsBuilder&) = delete;

  void run();

private:
  volatile std::sig_atomic_t* m_signal_status;
  Scheduler m_tasks;
  TsBuffer& m_timeslice_buffer;

  std::string m_manager_address;
  int64_t m_timeout_ns;
  static constexpr ucx::util::LoopMode m_ucx_loop_mode =
      ucx::util::LoopMode::busy_poll;
  std::string m_hostname;
  BuilderInfo m_builder_info;
  std::vector<std::byte> m_builder_info_bytes;
  cbm::Monitor* m_monitor = nullptr;

  int m_epoll_fd = -1;
  ucp_context_h m_context = nullptr;
  ucp_worker_h m_worker = nullptr;
  ucp_mem_h m_buffer_memh = nullptr;

  std::unordered_map<std::string, ucp_ep_h> m_sender_to_ep;
  std::unordered_map<ucp_ep_h, std::string> m_ep_to_sender;

  std::unordered_map<TsId, std::unique_ptr<TsHandle>> m_ts_handles;
  struct RecvRequestInfo {
    TsId id = 0;
    std::size_t ci = 0;
    uint64_t expected_size = 0;
  };
  std::unordered_map<ucs_status_ptr_t, RecvRequestInfo>
      m_active_data_recv_requests;

  static constexpr auto m_manager_retry_interval = 2s;
  bool m_mute_manager_reconnect = false;

  ucp_ep_h m_manager_ep = nullptr;
  bool m_manager_connecting = false;
  bool m_manager_connected = false;

  size_t m_timeslice_count = 0; ///< total number of assigned timeslices
  size_t m_component_count = 0; ///< total number of received components
  size_t m_byte_count = 0;      ///< total number of processed bytes
  size_t m_timeslice_incomplete_count = 0; ///< number of incomplete timeslices

  // Manager connection management
  void connect_to_manager_if_needed();
  void connect_to_manager();
  void handle_manager_error(ucp_ep_h ep, ucs_status_t status);
  void handle_manager_register_complete(ucs_status_ptr_t request,
                                        ucs_status_t status);
  void disconnect_from_manager(bool force = false);

  // Manager message handling
  void send_status_to_manager(uint64_t event, TsId id);
  void send_periodic_status_to_manager();
  void check_for_timeout(TsId id);
  ucs_status_t handle_manager_assign_ts(const void* header,
                                        size_t header_length,
                                        void* data,
                                        size_t length,
                                        const ucp_am_recv_param_t* param);

  // Sender connection management
  void connect_to_sender(const std::string& sender_id);
  void handle_sender_error(ucp_ep_h ep, ucs_status_t status);
  void disconnect_from_senders();

  // Sender message handling
  void post_tag_recvs(TsHandle& tsh, std::size_t ci);
  void cancel_data_recvs(TsId id, std::size_t ci);
  void complete_block(TsHandle& tsh, std::size_t ci);
  void
  send_request_to_sender(const std::string& sender_id, TsId id, std::size_t ci);
  void handle_sender_data_recv_complete(void* request,
                                        ucs_status_t status,
                                        size_t length);

  // Queue processing
  void process_completion(TsId id);

  // Helper methods
  void update_st_state(TsHandle& tsh,
                       std::size_t contribution_index,
                       StState new_state);
  static StDescriptor build_published_descriptor(TsHandle& tsh);
  void report_status();

  // UCX static callbacks (trampolines)
  static void on_manager_error(void* arg, ucp_ep_h ep, ucs_status_t status) {
    static_cast<TsBuilder*>(arg)->handle_manager_error(ep, status);
  }
  static void on_manager_register_complete(void* request,
                                           ucs_status_t status,
                                           void* user_data) {
    static_cast<TsBuilder*>(user_data)->handle_manager_register_complete(
        request, status);
  }
  static ucs_status_t on_manager_assign_ts(void* arg,
                                           const void* header,
                                           size_t header_length,
                                           void* data,
                                           size_t length,
                                           const ucp_am_recv_param_t* param) {
    return static_cast<TsBuilder*>(arg)->handle_manager_assign_ts(
        header, header_length, data, length, param);
  }
  static void on_sender_error(void* arg, ucp_ep_h ep, ucs_status_t status) {
    static_cast<TsBuilder*>(arg)->handle_sender_error(ep, status);
  }
  static void on_sender_data_recv_complete(void* request,
                                           ucs_status_t status,
                                           const ucp_tag_recv_info_t* info,
                                           void* user_data) {
    static_cast<TsBuilder*>(user_data)->handle_sender_data_recv_complete(
        request, status, info != nullptr ? info->length : 0);
  }
};
