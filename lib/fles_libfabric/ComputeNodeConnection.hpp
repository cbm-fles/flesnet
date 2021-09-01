// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#pragma once

#include "ComputeNodeInfo.hpp"
#include "ComputeNodeStatusMessage.hpp"
#include "Connection.hpp"
#include "InputChannelStatusMessage.hpp"
#include "InputNodeInfo.hpp"
#include "RequestIdentifier.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include "dfs/DDSchedulerOrchestrator.hpp"

#include <boost/format.hpp>
#include <cmath>
#include <rdma/fi_cm.h>

///-----
#include <map>
///-----/

namespace tl_libfabric {

/// Compute node connection class.
/** A ComputeNodeConnection object represents the endpoint of a single
 timeslice building connection from a compute node to an input
 node. */

class ComputeNodeConnection : public Connection {
public:
  ComputeNodeConnection(struct fid_eq* eq,
                        uint_fast16_t connection_index,
                        uint_fast16_t remote_connection_index,
                        InputNodeInfo remote_info,
                        uint8_t* data_ptr,
                        uint32_t data_buffer_size_exp,
                        fles::TimesliceComponentDescriptor* desc_ptr,
                        uint32_t desc_buffer_size_exp);

  ComputeNodeConnection(struct fid_eq* eq,
                        struct fid_domain* pd,
                        struct fid_cq* cq,
                        struct fid_av* av,
                        uint_fast16_t connection_index,
                        uint_fast16_t remote_connection_index,
                        /*InputNodeInfo remote_info, */ uint8_t* data_ptr,
                        uint32_t data_buffer_size_exp,
                        fles::TimesliceComponentDescriptor* desc_ptr,
                        uint32_t desc_buffer_size_exp);

  ComputeNodeConnection(const ComputeNodeConnection&) = delete;
  void operator=(const ComputeNodeConnection&) = delete;

  /// Post a receive work request (WR) to the receive queue
  void post_recv_status_message();

  void post_send_status_message();

  void post_send_final_status_message();

  void request_abort() { send_status_message_.request_abort = true; }

  bool abort_flag() { return recv_status_message_.abort; }

  void setup() override;

  void setup_mr(struct fid_domain* pd) override;

  bool try_sync_buffer_positions() override;

  void on_complete_heartbeat_recv() override;

  /// Connection handler function, called on successful connection.
  /**
   \param event RDMA connection manager event structure
   */
  void on_established(struct fi_eq_cm_entry* event) override;

  void on_disconnected(struct fi_eq_cm_entry* event) override;

  void inc_ack_pointers(uint64_t ack_pos);

  /// Handle Libfabric receive completion notification.
  void on_complete_recv() override;

  /// Handle Libfabric send completion notification.
  void on_complete_send() override;

  void on_complete_send_finalize();

  const ComputeNodeBufferPosition& cn_wp() const { return cn_wp_; }

  const ComputeNodeBufferPosition& cn_ack() const { return cn_ack_; }

  const InputChannelStatusMessage& recv_status_message() const {
    return recv_status_message_;
  }

  const HeartbeatMessage& recv_heartbeat_message() const {
    return recv_heartbeat_message_;
  }

  std::unique_ptr<std::vector<uint8_t>> get_private_data() override;

  struct BufferStatus {
    std::chrono::system_clock::time_point time;
    uint64_t size;

    uint64_t cached_acked;
    uint64_t acked;
    uint64_t received;

    int64_t used() const { return received - acked; }
    int64_t freeing() const { return acked - cached_acked; }
    int64_t unused() const { return cached_acked + size - received; }

    float percentage(int64_t value) const {
      return static_cast<float>(value) / static_cast<float>(size);
    }

    std::string caption() const { return std::string("used/freeing/free"); }

    std::string percentage_str(int64_t value) const {
      boost::format percent_fmt("%4.1f%%");
      percent_fmt % (percentage(value) * 100);
      std::string s = percent_fmt.str();
      s.resize(4);
      return s;
    }

    std::string percentages() const {
      return percentage_str(used()) + " " + percentage_str(freeing()) + " " +
             percentage_str(unused());
    }

    std::vector<int64_t> vector() const {
      return std::vector<int64_t>{used(), freeing(), unused()};
    }
  };

  BufferStatus buffer_status_data() const {
    return BufferStatus{std::chrono::system_clock::now(),
                        (UINT64_C(1) << data_buffer_size_exp_),
                        send_status_message_.ack.data, cn_ack_.data,
                        cn_wp_.data};
  }

  BufferStatus buffer_status_desc() const {
    return BufferStatus{std::chrono::system_clock::now(),
                        (UINT64_C(1) << desc_buffer_size_exp_),
                        send_status_message_.ack.desc, cn_ack_.desc,
                        cn_wp_.desc};
  }

  void set_partner_addr(fi_addr_t addr);

  void send_ep_addr();

  void set_remote_info(InputNodeInfo remote_info);

  bool is_connection_finalized();

private:
  /// Write the received timeslice descriptors from the sync messages in the
  /// memory (to minimize RDMA Writes)
  void write_received_descriptors();

  //
  void sync_after_scheduler_decision_received();

  void update_scheduler_interval_data();

  ComputeNodeStatusMessage send_status_message_ = ComputeNodeStatusMessage();
  ComputeNodeBufferPosition cn_ack_ = ComputeNodeBufferPosition();

  InputChannelStatusMessage recv_status_message_ = InputChannelStatusMessage();
  ComputeNodeBufferPosition cn_wp_ = ComputeNodeBufferPosition();

  struct fid_mr* mr_data_ = nullptr;
  struct fid_mr* mr_desc_ = nullptr;
  struct fid_mr* mr_send_ = nullptr;
  struct fid_mr* mr_recv_ = nullptr;

  /// Information on remote end.
  InputNodeInfo remote_info_{0};

  uint8_t* data_ptr_ = nullptr;
  const std::size_t data_buffer_size_exp_ = 0;

  fles::TimesliceComponentDescriptor* desc_ptr_ = nullptr;
  const std::size_t desc_buffer_size_exp_ = 0;

  /// Libfabric receive work request
  struct fi_msg_tagged recv_wr = fi_msg_tagged();

  /// Scatter/gather list entry for receive work request
  struct iovec recv_sge = iovec();
  void* recv_wr_descs[1] = {nullptr};

  /// Libfabric send work request
  struct fi_msg_tagged send_wr = fi_msg_tagged();

  /// Scatter/gather list entry for send work request
  struct iovec send_sge = iovec();
  void* send_wr_descs[1] = {nullptr};

  uint32_t pending_send_requests_{0};

  fi_addr_t partner_addr_ = -1;

  bool registered_input_MPI_time = false;

  /// To prevent sending more messages (late messages) once the final message
  /// is sent out
  bool final_msg_sent_ = false;
};
} // namespace tl_libfabric
