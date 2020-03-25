// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ComputeNodeStatusMessage.hpp"
#include "IBConnection.hpp"
#include "InputChannelStatusMessage.hpp"
#include "InputNodeInfo.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include <boost/format.hpp>
#include <chrono>

/// Compute node connection class.
/** A ComputeNodeConnection object represents the endpoint of a single
    timeslice building connection from a compute node to an input
    node. */

class ComputeNodeConnection : public IBConnection {
public:
  ComputeNodeConnection(struct rdma_event_channel* ec,
                        uint_fast16_t connection_index,
                        uint_fast16_t remote_connection_index,
                        struct rdma_cm_id* id,
                        InputNodeInfo remote_info,
                        uint8_t* data_ptr,
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

  void setup(struct ibv_pd* pd) override;

  /// Connection handler function, called on successful connection.
  /**
     \param event RDMA connection manager event structure
  */
  void on_established(struct rdma_cm_event* event) override;

  void on_disconnected(struct rdma_cm_event* event) override;

  void inc_ack_pointers(uint64_t ack_pos);

  void on_complete_recv();

  void on_complete_send();

  void on_complete_send_finalize();

  const ComputeNodeBufferPosition& cn_wp() const { return cn_wp_; }

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

    static std::string caption() { return std::string("used/freeing/free"); }

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

private:
  ComputeNodeStatusMessage send_status_message_ = ComputeNodeStatusMessage();
  ComputeNodeBufferPosition cn_ack_ = ComputeNodeBufferPosition();

  InputChannelStatusMessage recv_status_message_ = InputChannelStatusMessage();
  ComputeNodeBufferPosition cn_wp_ = ComputeNodeBufferPosition();

  struct ibv_mr* mr_data_ = nullptr;
  struct ibv_mr* mr_desc_ = nullptr;
  struct ibv_mr* mr_send_ = nullptr;
  struct ibv_mr* mr_recv_ = nullptr;

  /// Information on remote end.
  InputNodeInfo remote_info_{0};

  uint8_t* data_ptr_ = nullptr;
  std::size_t data_buffer_size_exp_ = 0;

  fles::TimesliceComponentDescriptor* desc_ptr_ = nullptr;
  std::size_t desc_buffer_size_exp_ = 0;

  /// InfiniBand receive work request
  ibv_recv_wr recv_wr = ibv_recv_wr();

  /// Scatter/gather list entry for receive work request
  ibv_sge recv_sge = ibv_sge();

  /// Infiniband send work request
  ibv_send_wr send_wr = ibv_send_wr();

  /// Scatter/gather list entry for send work request
  ibv_sge send_sge = ibv_sge();

  uint32_t pending_send_requests_{0};
};
