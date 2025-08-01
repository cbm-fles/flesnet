// Copyright 2025 Jan de Cuveland
#pragma once

// AM IDs for communication

// 1. tssched (listen) <-> stsender (connect)
// stsender -> tssched
static constexpr unsigned int AM_SENDER_REGISTER =
    20; // header: {sender_id}, data: none
static constexpr unsigned int AM_SENDER_ANNOUNCE_ST =
    21; // header: {StId, desc_size, content_size}, data: StDescriptor
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
    41; // header: {bytes_available, bytes_processed}, data: none
// tssched -> tsbuilder
static constexpr unsigned int AM_SCHED_SEND_TS =
    50; // header: {StId, desc_size, content_size}, data: StCollectionDescriptor

// 3. stsender (listen) <-> tsbuilder (connect)
// tsbuilder -> stsender
static constexpr unsigned int AM_BUILDER_REQUEST_ST =
    60; // header: {StId}, data: none
// stsender -> tsbuilder
static constexpr unsigned int AM_SENDER_SEND_ST =
    70; // header: {StId, desc_size, content_size}, data: {StDescriptor,
        // StContent}
