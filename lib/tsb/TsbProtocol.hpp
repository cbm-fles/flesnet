/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */
#pragma once

#include <cstdint>

static constexpr uint16_t DEFAULT_SCHEDULER_PORT = 13373;
static constexpr uint16_t DEFAULT_SENDER_PORT = 13374;

// AM IDs for communication

// 1. tssched (listen) <-> stsender (connect)
// stsender -> tssched
static constexpr unsigned int AM_SENDER_REGISTER =
    20; // header: {sender_id}, data: none
static constexpr unsigned int AM_SENDER_ANNOUNCE_ST =
    21; // header: {StId, ms_data_size},
        // data: serialized StDescriptor
static constexpr unsigned int AM_SENDER_RETRACT_ST =
    22; // header: {StId}, data: none
// tssched -> stsender
static constexpr unsigned int AM_SCHED_RELEASE_ST =
    30; // header: {StId}, data: none

// 2. tssched (listen) <-> tsbuilder (connect)
// tsbuilder -> tssched
static constexpr unsigned int AM_BUILDER_REGISTER =
    40; // header: {builder_id}, data: none
static constexpr unsigned int AM_BUILDER_STATUS =
    41; // header: {event, StId, size}, data: none
/*
   {event, StId, size}

   no_op, (--), bytes_free
   allocated, StId, new_bytes_free
   out_of_memory, StId, (--)
   received, StId, (--)
   released, StId, new_bytes_free
*/
// tssched -> tsbuilder
static constexpr unsigned int AM_SCHED_ASSIGN_TS =
    50; // header: {StId, ms_data_size},
        // data: serialized StCollection (incl. merged StDescriptor)

// 3. stsender (listen) <-> tsbuilder (connect)
// tsbuilder -> stsender
static constexpr unsigned int AM_BUILDER_REQUEST_ST =
    60; // header: {StId, tag}, data: none
//
// The actual bulk transfer stsender -> tsbuilder uses UCX tag matching
// (ucp_tag_send_nbx / ucp_tag_recv_nbx), not an active message. A
// contribution is transferred as a sequence of contiguous blocks -- two per
// component: the microslice descriptors, then the content -- each sent as
// one tagged message. All blocks of a contribution use the same tag; tag
// matching is FIFO per tag, so they complete the builder's pre-posted
// receives in order. Both sides derive the identical block layout
// independently: the sender from its announced descriptor, the builder from
// the merged descriptor relayed by the scheduler (descriptor block size =
// num_microslices * sizeof(MicrosliceDescriptor)). Contiguous blocks are
// sent with the default (contiguous) UCX datatype, which allows the
// single-RDMA-read rendezvous protocol; the iov datatype would force UCX
// into fragmented sends at roughly half the achievable bandwidth. The
// builder pre-posts the receives into its registered timeslice buffer
// immediately after receiving AM_SCHED_ASSIGN_TS and before issuing the
// request to the sender, so the recvs are "expected" by the time the sender
// starts sending, avoiding the rendezvous CTS round-trip.

// Tag encoding for the bulk sender->builder transfer.
// Layout: high 48 bits = TsId, low 16 bits = contribution (sender) index
// within the timeslice. The tag fully identifies a (TsId, sender) pair, so
// the receive can be posted with an exact-match tag_mask.
inline constexpr uint64_t make_st_data_tag(uint64_t ts_id,
                                           uint32_t contribution_index) {
  return (ts_id << 16) | (static_cast<uint64_t>(contribution_index) & 0xffffu);
}

static constexpr uint64_t BUILDER_EVENT_NO_OP = 0;
static constexpr uint64_t BUILDER_EVENT_ALLOCATED = 1;
static constexpr uint64_t BUILDER_EVENT_OUT_OF_MEMORY = 2;
static constexpr uint64_t BUILDER_EVENT_RECEIVED = 3;
static constexpr uint64_t BUILDER_EVENT_RELEASED = 4;
