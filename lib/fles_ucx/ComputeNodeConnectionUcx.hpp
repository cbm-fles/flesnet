// Copyright 2012-2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ComputeNodeStatusMessageUcx.hpp"
#include "IBConnectionUcx.hpp"
#include "InputChannelStatusMessageUcx.hpp"
#include "InputNodeInfoUcx.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include <boost/format.hpp>
#include <chrono>

/// Compute node connection class.
/** A ComputeNodeConnection object represents the endpoint of a single
    timeslice building connection from a compute node to an input
    node. */

class ComputeNodeConnectionUcx : public IBConnectionUcx {
public:
  ComputeNodeConnectionUcx(struct rdma_event_channel* ec,
                           uint_fast16_t connection_index,
                           uint_fast16_t remote_connection_index,
                           struct rdma_cm_id* id,
                           InputNodeInfoUcx remote_info,
                           uint8_t* data_ptr,
                           uint32_t data_buffer_size_exp,
                           fles::TimesliceComponentDescriptor* desc_ptr,
                           uint32_t desc_buffer_size_exp);

  ComputeNodeConnectionUcx(const ComputeNodeConnectionUcx&) = delete;
  void operator=(const ComputeNodeConnectionUcx&) = delete;

  /// Post a receive work request (WR) to the receive queue
  void post_recv_status_message();

  void post_send_status_message();

  void post_send_final_status_message();

  void request_abort() { send_status_message_.request_abort = true; }

  [[nodiscard]] bool abort_flag() const { return recv_status_message_.abort; }

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

  [[nodiscard]] const ComputeNodeBufferPositionUcx& cn_wp() const {
    return cn_wp_;
  }

  std::unique_ptr<std::vector<uint8_t>> get_private_data() override;

  struct BufferStatus {
    std::chrono::system_clock::time_point time;
    uint64_t size = 0;

    uint64_t cached_acked = 0;
    uint64_t acked = 0;
    uint64_t received = 0;

    [[nodiscard]] int64_t used() const { return received - acked; }
    [[nodiscard]] int64_t freeing() const { return acked - cached_acked; }
    [[nodiscard]] int64_t unused() const {
      return cached_acked + size - received;
    }

    [[nodiscard]] float percentage(int64_t value) const {
      return static_cast<float>(value) / static_cast<float>(size);
    }

    static std::string caption() { return {"used/freeing/free"}; }

    [[nodiscard]] std::string percentage_str(int64_t value) const {
      boost::format percent_fmt("%4.1f%%");
      percent_fmt % (percentage(value) * 100);
      std::string s = percent_fmt.str();
      s.resize(4);
      return s;
    }

    [[nodiscard]] std::string percentages() const {
      return percentage_str(used()) + " " + percentage_str(freeing()) + " " +
             percentage_str(unused());
    }

    [[nodiscard]] std::vector<int64_t> vector() const {
      return std::vector<int64_t>{used(), freeing(), unused()};
    }
  };

  [[nodiscard]] BufferStatus buffer_status_data() const {
    return BufferStatus{std::chrono::system_clock::now(),
                        (UINT64_C(1) << data_buffer_size_exp_),
                        send_status_message_.ack.data, cn_ack_.data,
                        cn_wp_.data};
  }

  [[nodiscard]] BufferStatus buffer_status_desc() const {
    return BufferStatus{std::chrono::system_clock::now(),
                        (UINT64_C(1) << desc_buffer_size_exp_),
                        send_status_message_.ack.desc, cn_ack_.desc,
                        cn_wp_.desc};
  }

private:
  ComputeNodeStatusMessageUcx send_status_message_ =
      ComputeNodeStatusMessageUcx();
  ComputeNodeBufferPositionUcx cn_ack_ = ComputeNodeBufferPositionUcx();

  InputChannelStatusMessageUcx recv_status_message_ =
      InputChannelStatusMessageUcx();
  ComputeNodeBufferPositionUcx cn_wp_ = ComputeNodeBufferPositionUcx();

  struct ibv_mr* mr_data_ = nullptr;
  struct ibv_mr* mr_desc_ = nullptr;
  struct ibv_mr* mr_send_ = nullptr;
  struct ibv_mr* mr_recv_ = nullptr;

  /// Information on remote end.
  InputNodeInfoUcx remote_info_{0};

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
