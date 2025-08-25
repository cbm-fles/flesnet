// Copyright 2025 Jan de Cuveland

#include "TsBuilder.hpp"
#include "SubTimeslice.hpp"
#include "System.hpp"
#include "TsbProtocol.hpp"
#include "log.hpp"
#include "monitoring/System.hpp"
#include "ucxutil.hpp"
#include <arpa/inet.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <sched.h>
#include <span>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <ucp/api/ucp.h>
#include <ucp/api/ucp_compat.h>
#include <ucs/type/status.h>

TsBuilder::TsBuilder(TimesliceBufferFlex& timeslice_buffer,
                     std::string_view scheduler_address,
                     int64_t timeout_ns)
    : timeslice_buffer_(timeslice_buffer),
      scheduler_address_(scheduler_address), timeout_ns_{timeout_ns} {
  builder_id_ = fles::system::current_hostname() + ":" +
                std::to_string(fles::system::current_pid()); // TODO

  // Initialize event handling
  epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
  if (epoll_fd_ == -1) {
    throw std::runtime_error("epoll_create1 failed");
  }

  // Start the worker thread
  worker_thread_ = std::jthread([this](std::stop_token st) { (*this)(st); });
}

TsBuilder::~TsBuilder() {
  worker_thread_.request_stop();

  if (epoll_fd_ != -1) {
    close(epoll_fd_);
  }
}

// Main operation loop

void TsBuilder::operator()(std::stop_token stop_token) {
  cbm::system::set_thread_name("TsBuilder");

  if (!ucx::util::init(context_, worker_, epoll_fd_)) {
    L_(error) << "Failed to initialize UCX";
    return;
  }
  if (!ucx::util::set_receive_handler(worker_, AM_SCHED_SEND_TS,
                                      on_scheduler_send_ts, this) ||
      !ucx::util::set_receive_handler(worker_, AM_SENDER_SEND_ST,
                                      on_sender_data, this)) {
    L_(error) << "Failed to register receive handlers";
    return;
  }
  connect_to_scheduler_if_needed();

  while (!stop_token.stop_requested()) {
    if (ucp_worker_progress(worker_) != 0) {
      continue;
    }
    if (auto id = timeslice_buffer_.try_receive_completion()) {
      process_completion(*id);
      continue;
    }
    tasks_.timer();

    if (!ucx::util::arm_worker_and_wait(worker_, epoll_fd_)) {
      break;
    }
  }

  disconnect_from_scheduler();
  disconnect_from_senders();
  ucx::util::cleanup(context_, worker_);
}

// Scheduler connection management

void TsBuilder::connect_to_scheduler_if_needed() {
  constexpr auto interval = std::chrono::seconds(2);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  if (!scheduler_connecting_ &&
      !worker_thread_.get_stop_token().stop_requested()) {
    connect_to_scheduler();
  }

  tasks_.add([this] { connect_to_scheduler_if_needed(); }, now + interval);
}

void TsBuilder::connect_to_scheduler() {
  if (scheduler_connecting_ || scheduler_connected_) {
    return;
  }

  auto [address, port] =
      ucx::util::parse_address(scheduler_address_, DEFAULT_SCHEDULER_PORT);
  auto ep =
      ucx::util::connect(worker_, address, port, on_scheduler_error, this);
  if (!ep) {
    L_(error) << "Failed to connect to scheduler at " << address << ":" << port;
    return;
  }

  scheduler_ep_ = *ep;

  if (!register_with_scheduler()) {
    L_(error) << "Failed to register with scheduler";
    disconnect_from_scheduler(true);
    return;
  }

  scheduler_connecting_ = true;
}

void TsBuilder::handle_scheduler_error(ucp_ep_h ep, ucs_status_t status) {
  if (ep != scheduler_ep_) {
    L_(error) << "Received error for unknown endpoint: "
              << ucs_status_string(status);
    return;
  }

  disconnect_from_scheduler(true);
  L_(info) << "Disconnected from scheduler: " << ucs_status_string(status);
}

bool TsBuilder::register_with_scheduler() {
  auto header = std::as_bytes(std::span(builder_id_));
  return ucx::util::send_active_message(
      scheduler_ep_, AM_SENDER_REGISTER, header, {},
      on_scheduler_register_complete, this, 0);
}

void TsBuilder::handle_scheduler_register_complete(ucs_status_ptr_t request,
                                                   ucs_status_t status) {
  scheduler_connecting_ = false;

  if (status != UCS_OK) {
    L_(error) << "Failed to register with scheduler: "
              << ucs_status_string(status);
  } else {
    scheduler_connected_ = true;
    L_(info) << "Successfully registered with scheduler";
  }

  if (request != nullptr) {
    ucp_request_free(request);
  }
};

void TsBuilder::disconnect_from_scheduler(bool force) {
  scheduler_connecting_ = false;
  scheduler_connected_ = false;

  if (scheduler_ep_ == nullptr) {
    return;
  }

  ucx::util::close_endpoint(worker_, scheduler_ep_, force);
  scheduler_ep_ = nullptr;
  L_(debug) << "Disconnected from scheduler";
}

// Scheduler message handling

void TsBuilder::send_status_to_scheduler(uint64_t event, TsID id) {
  uint64_t bytes_free = timeslice_buffer_.get_free_memory();
  std::array<uint64_t, 3> hdr{event, id, bytes_free};
  auto header = std::as_bytes(std::span(hdr));

  ucx::util::send_active_message(scheduler_ep_, AM_BUILDER_STATUS, header, {},
                                 ucx::util::on_generic_send_complete, this,
                                 UCP_AM_SEND_FLAG_COPY_HEADER);
}

ucs_status_t TsBuilder::handle_scheduler_send_ts(
    const void* header,
    size_t header_length,
    void* data,
    size_t length,
    [[maybe_unused]] const ucp_am_recv_param_t* param) {
  L_(trace) << "Received TS from scheduler with header length " << header_length
            << " and data length " << length;

  auto hdr = std::span<const uint64_t>(static_cast<const uint64_t*>(header),
                                       header_length / sizeof(uint64_t));
  if (hdr.size() != 3 || length == 0) {
    L_(error) << "Invalid scheduler TS received";
    return UCS_OK;
  }

  const uint64_t id = hdr[0];
  const uint64_t desc_size = hdr[1];
  const uint64_t content_size = hdr[2];

  if (desc_size != length) {
    L_(error) << "Invalid header data in scheduler TS";
    return UCS_OK;
  }

  // Try to allocate memory for the content
  auto* buffer = timeslice_buffer_.allocate(content_size);
  if (buffer == nullptr) {
    L_(error) << "Failed to allocate memory for TS with ID: " << id;
    send_status_to_scheduler(BUILDER_EVENT_OUT_OF_MEMORY, id);
    return UCS_OK;
  }

  // TODO: add error handling for deserialization
  auto desc = to_obj<StCollectionDescriptor>(
      std::span(static_cast<const std::byte*>(data), desc_size));
  ts_handlers_.emplace_back(
      std::make_unique<TsHandler>(buffer, std::move(desc)));
  auto& tsh = *ts_handlers_.back();
  send_status_to_scheduler(BUILDER_EVENT_ALLOCATED, id);

  L_(debug) << "Received TS from scheduler with ID: " << id
            << ", desc size: " << desc_size
            << ", content size: " << content_size;

  // Ask senders for the contributions
  for (std::size_t i = 0; i < tsh.contributions.size(); ++i) {
    send_request_to_sender(tsh.contributions[i].sender_id, id);
    update_st_state(tsh, i, StState::Requested);
  }

  return UCS_OK;
}

// Sender connection management

void TsBuilder::connect_to_sender(std::string sender_id) {
  auto [address, port] =
      ucx::util::parse_address(sender_id, DEFAULT_SENDER_PORT);
  auto ep =
      ucx::util::connect(worker_, address, port, on_scheduler_error, this);
  if (!ep) {
    L_(error) << "Failed to connect to sender at " << address << ":" << port;
    return;
  }

  senders_.emplace_back(sender_id, *ep);
}

void TsBuilder::handle_sender_error(ucp_ep_h ep, ucs_status_t status) {
  auto it =
      std::find_if(senders_.begin(), senders_.end(),
                   [ep](const SenderConnection& s) { return s.ep == ep; });
  if (it == senders_.end()) {
    L_(error) << "Received error for unknown sender endpoint: "
              << ucs_status_string(status);
    return;
  }
  ucx::util::close_endpoint(worker_, it->ep, true);
  senders_.erase(it);
  L_(info) << "Sender " << it->sender_id
           << " disconnected: " << ucs_status_string(status);
}

void TsBuilder::disconnect_from_senders() {
  for (auto& sender : senders_) {
    ucx::util::close_endpoint(worker_, sender.ep, true);
  }
  senders_.clear();
  L_(info) << "Disconnected from all senders";
}

// Sender message handling

void TsBuilder::send_request_to_sender(std::string_view sender_id, TsID id) {
  auto it = std::find_if(senders_.begin(), senders_.end(),
                         [sender_id](const SenderConnection& s) {
                           return s.sender_id == sender_id;
                         });

  if (it == senders_.end()) {
    L_(debug) << "Connecting to sender: " << sender_id;
    connect_to_sender(std::string(sender_id));

    it = std::find_if(senders_.begin(), senders_.end(),
                      [sender_id](const SenderConnection& s) {
                        return s.sender_id == sender_id;
                      });

    if (it == senders_.end()) {
      return;
    }
  }

  std::array<uint64_t, 1> hdr{id};
  auto header = std::as_bytes(std::span(hdr));

  ucx::util::send_active_message(it->ep, AM_BUILDER_REQUEST_ST, header, {},
                                 ucx::util::on_generic_send_complete, this,
                                 UCP_AM_SEND_FLAG_COPY_HEADER);
}

ucs_status_t TsBuilder::handle_sender_data(const void* header,
                                           size_t header_length,
                                           void* data,
                                           size_t length,
                                           const ucp_am_recv_param_t* param) {
  L_(trace) << "Received sender ST data with header length " << header_length
            << " and data length " << length;

  auto hdr = std::span<const uint64_t>(static_cast<const uint64_t*>(header),
                                       header_length / sizeof(uint64_t));
  if (hdr.size() != 3 || length == 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FLAG_RNDV) == 0u) {
    L_(error) << "Invalid sender ST data received";
    return UCS_OK;
  }

  const uint64_t id = hdr[0];
  const uint64_t desc_size = hdr[1];
  const uint64_t content_size = hdr[2];

  if (desc_size != length) {
    L_(error) << "Invalid header data in sender ST announcement";
    return UCS_OK;
  }

  // Check if we have a sender connection for this endpoint
  ucp_ep_h ep = param->reply_ep;
  auto it =
      std::find_if(senders_.begin(), senders_.end(),
                   [ep](const SenderConnection& s) { return s.ep == ep; });
  if (it == senders_.end()) {
    L_(error) << "Received ST data from unknown sender";
    return UCS_OK;
  }
  auto& sender_id = it->sender_id;

  // Check if we have a handler for this TS id
  auto ts_it = std::find_if(ts_handlers_.begin(), ts_handlers_.end(),
                            [id](const auto& ts) { return ts->id == id; });
  if (ts_it == ts_handlers_.end()) {
    L_(error) << "Received ST data for unknown TS ID: " << id;
    return UCS_OK;
  }
  auto& tsh = **ts_it;

  // Check if we have a contribution for this sender
  auto contribution_it =
      std::find_if(tsh.contributions.begin(), tsh.contributions.end(),
                   [sender_id](const TsContribution& c) {
                     return c.sender_id == sender_id;
                   });
  if (contribution_it == tsh.contributions.end()) {
    L_(error) << "Received ST data from unknown sender: " << sender_id
              << " for TS ID: " << id;
    return UCS_OK;
  }
  std::size_t contribution_index =
      std::distance(tsh.contributions.begin(), contribution_it);

  if (desc_size != contribution_it->desc_size ||
      content_size != contribution_it->content_size) {
    L_(error) << "Invalid ST data sizes from sender " << sender_id
              << " for TS ID: " << id
              << ", expected desc size: " << contribution_it->desc_size
              << ", content size: " << contribution_it->content_size
              << ", received desc size: " << desc_size
              << ", content size: " << content_size;
    update_st_state(tsh, contribution_index, StState::Failed);
    return UCS_OK;
  }

  // RNDV receive: First desc_size bytes are the serialized StDescriptor,
  // remaining bytes are the content
  ucp_request_param_t req_param{};
  req_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                           UCP_OP_ATTR_FIELD_USER_DATA |
                           UCP_OP_ATTR_FIELD_DATATYPE;
  req_param.cb.recv_am = on_sender_data_recv_complete;
  req_param.user_data = this;
  req_param.datatype = ucp_dt_make_iov();

  update_st_state(tsh, contribution_index, StState::Receiving);
  ucs_status_ptr_t request = ucp_am_recv_data_nbx(
      worker_, data, tsh.iovectors[contribution_index].data(),
      tsh.iovectors[contribution_index].size(), &req_param);

  if (UCS_PTR_IS_ERR(request)) {
    L_(error) << "Failed to receive ST data from sender " << sender_id
              << " for TS ID: " << id
              << ", error: " << ucs_status_string(UCS_PTR_STATUS(request));
    update_st_state(tsh, contribution_index, StState::Failed);
    return UCS_OK;
  }

  if (request == nullptr) {
    // Operation has completed successfully in-place
    update_st_state(tsh, contribution_index, StState::Complete);
    L_(debug) << "Received ST data from sender " << sender_id
              << " for TS ID: " << id << ", desc size: " << desc_size
              << ", content size: " << content_size;
    return UCS_OK;
  }

  active_data_recv_requests_[request] = {id, contribution_index};
  L_(debug) << "Started receiving ST data from sender " << sender_id;

  return UCS_OK;
}

void TsBuilder::handle_sender_data_recv_complete(
    void* request, ucs_status_t status, [[maybe_unused]] size_t length) {
  if (UCS_PTR_IS_ERR(request)) {
    L_(error) << "Data recv operation failed: " << ucs_status_string(status);
  } else if (status != UCS_OK) {
    L_(error) << "Data recv operation completed with status: "
              << ucs_status_string(status);
  } else {
    L_(trace) << "Data recv operation completed successfully";
  }

  auto it = active_data_recv_requests_.find(request);
  if (it == active_data_recv_requests_.end()) {
    L_(error) << "Received completion for unknown data recv request";
  } else {
    auto [id, contribution_index] = it->second;
    auto ts_it = std::find_if(ts_handlers_.begin(), ts_handlers_.end(),
                              [id](const auto& ts) { return ts->id == id; });
    if (ts_it == ts_handlers_.end()) {
      L_(error) << "Received completion for unknown TS ID: " << id;
      return;
    }
    auto& tsh = **ts_it;
    if (contribution_index >= tsh.contributions.size()) {
      L_(error) << "Received completion for unknown contribution index: "
                << contribution_index << " for TS ID: " << id;
      return;
    }
    update_st_state(tsh, contribution_index, StState::Complete);
    active_data_recv_requests_.erase(request);
  }

  if (request != nullptr) {
    ucp_request_free(request);
  }
}

// Queue processing

void TsBuilder::process_completion(TsID id) {
  auto ts_it = std::find_if(ts_handlers_.begin(), ts_handlers_.end(),
                            [id](const auto& ts) { return ts->id == id; });
  if (ts_it == ts_handlers_.end()) {
    L_(error) << "Received completion for unknown TS ID: " << id;
    return;
  }
  timeslice_buffer_.deallocate((*ts_it)->buffer);
  ts_handlers_.erase(ts_it);
  send_status_to_scheduler(BUILDER_EVENT_RELEASED, id);
  L_(debug) << "Processed TS completion for ID: " << id;
}

// Helper methods

void TsBuilder::update_st_state(TsHandler& tsh,
                                std::size_t contribution_index,
                                StState new_state) {
  assert(contribution_index < states.size());
  if (new_state == tsh.states[contribution_index]) {
    return;
  }
  tsh.states[contribution_index] = new_state;
  if (new_state == StState::Complete) {
    // Deserialize the descriptor
    tsh.descriptors[contribution_index] = to_obj<StDescriptor>(
        std::span(tsh.serialized_descriptors[contribution_index]));
    // TODO: add error handling for deserialization
  }
  if (new_state == StState::Complete || new_state == StState::Failed) {
    if (std::all_of(tsh.states.begin(), tsh.states.end(), [](StState state) {
          return state == StState::Complete || state == StState::Failed;
        })) {
      // All contributions are complete (or failed), publish the timeslice
      if (!tsh.is_published) {
        StDescriptor ts_desc = create_timeslice_descriptor(tsh);
        timeslice_buffer_.send_work_item(tsh.buffer, tsh.id, ts_desc);
        tsh.is_published = true;
        L_(debug) << "Published TS with ID: " << tsh.id;
      }
    }
    // TODO: add timeout handling for incomplete timeslices
  }
}

StDescriptor TsBuilder::create_timeslice_descriptor(TsHandler& tsh) {
  StDescriptor d{};
  for (std::size_t i = 0; i < tsh.contributions.size(); ++i) {
    if (tsh.states[i] != StState::Complete) {
      d.set_flag(StFlag::MissingSubtimeslices);
      continue;
    }
    const auto& contrib = tsh.descriptors[i];
    if (d.duration_ns == 0) {
      d.start_time_ns = contrib.start_time_ns;
      d.duration_ns = contrib.duration_ns;
    } else if (d.start_time_ns != contrib.start_time_ns ||
               d.duration_ns != contrib.duration_ns) {
      L_(error) << "Inconsistent start time or duration in contributions";
      d.set_flag(StFlag::MissingSubtimeslices);
      continue;
    }
    d.flags |= contrib.flags;
    for (const auto& c : contrib.components) {
      d.components.push_back(c);
      d.components.back().descriptor.offset += tsh.offsets[i];
      d.components.back().content.offset += tsh.offsets[i];
    }
  }
  return d;
}
