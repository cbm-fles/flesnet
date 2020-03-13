// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "MicrosliceTransmitter.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <thread>

namespace fles {

MicrosliceTransmitter::MicrosliceTransmitter(
    InputBufferWriteInterface& data_sink)
    : data_sink_(data_sink) {}

bool MicrosliceTransmitter::try_put(
    const std::shared_ptr<const Microslice>& item) {
  assert(item != nullptr);
  const DualIndex item_size = {1, item->desc().size};
  const DualIndex buffer_size = {data_sink_.desc_buffer().size(),
                                 data_sink_.data_buffer().size()};
  DualIndex available = buffer_size - write_index_ + read_index_cached_;

  // DualIndex comparison means both components have to fulfill condition,
  // i.e., (a < b) is not the same as (!(a >= b)).
  if (!(available >= item_size)) {
    read_index_cached_ = data_sink_.get_read_index();
    available = buffer_size - write_index_ + read_index_cached_;
  }
  if (!(available >= item_size)) {
    return false;
  }

  uint8_t* const data_begin = &data_sink_.data_buffer().at(write_index_.data);

  uint8_t* const data_end =
      &data_sink_.data_buffer().at(write_index_.data + item_size.data);

  if (data_begin <= data_end) {
    std::copy_n(item->content(), item_size.data, data_begin);
  } else {
    size_t part1_size =
        buffer_size.data -
        (write_index_.data & data_sink_.data_buffer().size_mask());

    // copy data into two segments
    std::copy_n(item->content(), part1_size, data_begin);
    std::copy_n(item->content() + part1_size, item_size.data - part1_size,
                data_sink_.data_buffer().ptr());
  }

  data_sink_.desc_buffer().at(write_index_.desc) = item->desc();
  data_sink_.desc_buffer().at(write_index_.desc).offset = write_index_.data;

  write_index_ += item_size;
  data_sink_.set_write_index(write_index_);

  return true;
}

void MicrosliceTransmitter::put(std::shared_ptr<const Microslice> item) {
  while (!try_put(item)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}
} // namespace fles
