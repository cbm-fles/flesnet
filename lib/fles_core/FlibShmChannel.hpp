#pragma once

#include "DataSource.hpp"
#include "RingBuffer.hpp"
#include "RingBufferView.hpp"
#include "MicrosliceDescriptor.hpp"
#include <algorithm>

#include "shm_channel_client.hpp"

/// Wrapper around FLIB shared memory channel.
class FlibShmChannel : public DataSource
{
public:
    /// The FlibShmChannel constructor.
    FlibShmChannel(shm_channel_client* channel);

    FlibShmChannel(const FlibShmChannel&) = delete;
    void operator=(const FlibShmChannel&) = delete;

    virtual ~FlibShmChannel();

    virtual RingBufferView<volatile uint8_t>& data_buffer() override
    {
        return *_data_buffer_view;
    }

    virtual RingBufferView<volatile fles::MicrosliceDescriptor>&
    desc_buffer() override
    {
        return *_desc_buffer_view;
    }

    virtual uint64_t written_mc() override;
    virtual uint64_t written_data() override;

    virtual void update_ack_pointers(uint64_t new_acked_data,
                                     uint64_t new_acked_mc) override;

private:
    shm_channel_client* _channel;

    std::unique_ptr<RingBufferView<volatile uint8_t>> _data_buffer_view;
    std::unique_ptr<RingBufferView<volatile fles::MicrosliceDescriptor>>
        _desc_buffer_view;
};
