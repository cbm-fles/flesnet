#include "FlibShmChannel.hpp"
#include <boost/date_time/posix_time/posix_time_types.hpp>

FlibShmChannel::FlibShmChannel(shm_channel_client* channel) : channel_(channel)
{

    // convert sizes from 'bytes' to 'entries'
    size_t data_buffer_size_exp = channel_->data_buffer_size_exp();
    constexpr std::size_t microslice_descriptor_size_bytes_exp = 5;
    size_t desc_buffer_size_exp =
        channel_->desc_buffer_size_exp() - microslice_descriptor_size_bytes_exp;

    uint8_t* data_buffer = reinterpret_cast<uint8_t*>(channel_->data_buffer());
    fles::MicrosliceDescriptor* desc_buffer =
        reinterpret_cast<fles::MicrosliceDescriptor*>(channel_->desc_buffer());

    data_buffer_view_ = std::unique_ptr<RingBufferView<volatile uint8_t>>(
        new RingBufferView<volatile uint8_t>(data_buffer,
                                             data_buffer_size_exp));
    desc_buffer_view_ =
        std::unique_ptr<RingBufferView<volatile fles::MicrosliceDescriptor>>(
            new RingBufferView<volatile fles::MicrosliceDescriptor>(
                desc_buffer, desc_buffer_size_exp));
}

FlibShmChannel::~FlibShmChannel() {}

uint64_t FlibShmChannel::written_desc()
{
    return channel_->get_offsets_newer_than(
                       boost::posix_time::milliseconds(100))
        .first.desc_offset;
}

uint64_t FlibShmChannel::written_data()
{
    return channel_->get_offsets_newer_than(
                       boost::posix_time::milliseconds(100))
        .first.data_offset;
}

void FlibShmChannel::update_ack_pointers(uint64_t new_acked_data,
                                         uint64_t new_acked_desc)
{
    ack_ptrs_t ptrs;
    ptrs.data_ptr = new_acked_data & data_buffer_view_->size_mask();
    ptrs.desc_ptr = (new_acked_desc & desc_buffer_view_->size_mask()) *
                    sizeof(fles::MicrosliceDescriptor);
    channel_->set_ack_ptrs(ptrs);
}
