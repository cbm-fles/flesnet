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

DualRingBufferIndex FlibShmChannel::get_write_index()
{
    auto temp = channel_->get_write_index_newer_than(
                            boost::posix_time::milliseconds(100))
                    .first.index;

    return {temp.desc, temp.data};
}

void FlibShmChannel::set_read_index(DualRingBufferIndex new_read_index)
{
    DualIndex read_index;
    read_index.data = new_read_index.data & data_buffer_view_->size_mask();
    read_index.desc = (new_read_index.desc & desc_buffer_view_->size_mask()) *
                      sizeof(fles::MicrosliceDescriptor);
    channel_->set_read_index(read_index);
}
