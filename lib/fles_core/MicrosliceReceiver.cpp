// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "MicrosliceReceiver.hpp"
#include <chrono>
#include <thread>

namespace fles {

MicrosliceReceiver::MicrosliceReceiver(InputBufferReadInterface& data_source)
    : data_source_(data_source),
      write_index_desc_(data_source_.get_write_index().desc),
      read_index_desc_(data_source_.get_read_index().desc) {}

StorableMicroslice* MicrosliceReceiver::try_get() {
  // update write_index if needed
  if (write_index_desc_ <= read_index_desc_) {
    write_index_desc_ = data_source_.get_write_index().desc;
  }
  if (write_index_desc_ > read_index_desc_) {

    const MicrosliceDescriptor& desc =
        data_source_.desc_buffer().at(read_index_desc_);

    const uint8_t* data_begin = &data_source_.data_buffer().at(desc.offset);

    const uint64_t offset_end = desc.offset + desc.size;

    const uint8_t* data_end = &data_source_.data_buffer().at(offset_end);

    StorableMicroslice* sms;

    if (data_begin <= data_end) {
      // NOLINTNEXTLINE
      sms = new StorableMicroslice(
          const_cast<const fles::MicrosliceDescriptor&>(desc),
          const_cast<const uint8_t*>(data_begin));
    } else {
      const uint8_t* buffer_begin = data_source_.data_buffer().ptr();

      const uint8_t* buffer_end =
          buffer_begin + data_source_.data_buffer().bytes();

      // copy two segments to vector
      std::vector<uint8_t> data;
      data.reserve(desc.size);
      data.assign(data_begin, buffer_end);
      data.insert(data.end(), buffer_begin, data_end);
      assert(data.size() == desc.size);

      // NOLINTNEXTLINE
      sms = new StorableMicroslice(
          const_cast<const fles::MicrosliceDescriptor&>(desc), data);
    }

    ++read_index_desc_;

    data_source_.set_read_index({read_index_desc_, offset_end});

    return sms;
  }
  return nullptr;
}

StorableMicroslice* MicrosliceReceiver::do_get() {
  if (eos_) {
    return nullptr;
  }

  // wait until a microslice is available in the input buffer
  StorableMicroslice* sms = nullptr;
  while (sms == nullptr) {
    data_source_.proceed();
    sms = try_get();
    if (sms == nullptr) {
      if (data_source_.get_eof() &&
          read_index_desc_ == data_source_.get_write_index().desc) {
        eos_ = true;
        return nullptr;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  return sms;
}
} // namespace fles
