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
        // data: StDescriptor
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
    50; // header: {StId, ms_data_size}, data: StCollectionDescriptor

// 3. stsender (listen) <-> tsbuilder (connect)
// tsbuilder -> stsender
static constexpr unsigned int AM_BUILDER_REQUEST_ST =
    60; // header: {StId}, data: none
// stsender -> tsbuilder
static constexpr unsigned int AM_SENDER_SEND_ST =
    70; // header: {StId, sizeof(serialized StDescriptor), ms_data_size},
        // data: {StDescriptor, ms_data}

static constexpr uint64_t BUILDER_EVENT_NO_OP = 0;
static constexpr uint64_t BUILDER_EVENT_ALLOCATED = 1;
static constexpr uint64_t BUILDER_EVENT_OUT_OF_MEMORY = 2;
static constexpr uint64_t BUILDER_EVENT_RECEIVED = 3;
static constexpr uint64_t BUILDER_EVENT_RELEASED = 4;
