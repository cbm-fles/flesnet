#include "FlibShmChannel.hpp"
#include <boost/date_time/posix_time/posix_time_types.hpp>

FlibShmChannel::FlibShmChannel(shm_channel_client* channel)
    : _channel(channel)
{

  // convert sizes from 'bytes' to 'entries'
  size_t data_buffer_size_exp = _channel->data_buffer_size_exp();
  constexpr std::size_t microslice_descriptor_size_bytes_exp = 5;
  size_t desc_buffer_size_exp =
    _channel->desc_buffer_size_exp() - microslice_descriptor_size_bytes_exp;
  
    uint8_t* data_buffer =
        reinterpret_cast<uint8_t*>(_channel->data_buffer());
    fles::MicrosliceDescriptor* desc_buffer =
        reinterpret_cast<fles::MicrosliceDescriptor*>(
	     _channel->desc_buffer());

    _data_buffer_view = std::unique_ptr<RingBufferView<volatile uint8_t>>(
        new RingBufferView<volatile uint8_t>(data_buffer,
                                             data_buffer_size_exp));
    _desc_buffer_view =
        std::unique_ptr<RingBufferView<volatile fles::MicrosliceDescriptor>>(
            new RingBufferView<volatile fles::MicrosliceDescriptor>(
                desc_buffer, desc_buffer_size_exp));

}

FlibShmChannel::~FlibShmChannel() {}

uint64_t FlibShmChannel::written_mc()
{
  return _channel->get_offsets_newer_than(
      boost::posix_time::milliseconds(100)).desc_offset;
}

uint64_t FlibShmChannel::written_data()
{
  return _channel->get_offsets_newer_than(
      boost::posix_time::milliseconds(100)).data_offset;
}

void FlibShmChannel::update_ack_pointers(uint64_t new_acked_data,
                                         uint64_t new_acked_mc)
{
  ack_ptrs_t ptrs;
  ptrs.data_ptr = new_acked_data & _data_buffer_view->size_mask();
  ptrs.desc_ptr = (new_acked_mc & _desc_buffer_view->size_mask()) *
    sizeof(fles::MicrosliceDescriptor);
  _channel->set_ack_ptrs(ptrs);
}
