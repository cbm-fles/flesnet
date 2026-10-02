#include "TsclientWriter.hpp"
#include "MicrosliceDescriptor.hpp"
#include "OptionValues.hpp"
#include "SubTimeslice.hpp"
#include "Utility.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>

using namespace std;
using namespace std::chrono;

namespace {

/// The number of microslices in the data of a component, which holds the
/// microslice descriptors followed by the microslice contents
uint64_t count_microslices(const std::byte* data, uint64_t size) {
  uint64_t count = 0;
  uint64_t used = 0; // size of the descriptors seen so far and their contents
  while (used + sizeof(fles::MicrosliceDescriptor) <= size) {
    fles::MicrosliceDescriptor desc{};
    memcpy(&desc, data + count * sizeof(desc), sizeof(desc));
    used += sizeof(desc) + desc.size;
    count++;
  }
  if (used != size) {
    throw runtime_error("(TsclientWriter) inconsistent component data");
  }
  return count;
}

} // namespace

TsclientWriter::TsclientWriter(std::string output_uri) {
  UriComponents uri{output_uri};
  std::size_t size = UINT64_C(1) << 30; // 1 GiB
  const auto shm_identifier = uri.path;

  for (auto& [key, value] : uri.query_components) {
    if (key == "size") {
      size = fles::SizeValue::parse(value);
    } else {
      throw runtime_error(
          "Query parameter not implemented for scheme " + uri.scheme +
          ": " + key);
    }
  }

  ts_buffer_ = make_unique<fles::TimesliceShmBuffer>(zmq_context_, shm_identifier, size);
  // The buffer map manages a single block spanning the buffer
  buffer_ = ts_buffer_->allocate(size);
  if (buffer_ == nullptr) {
    throw runtime_error("(TsclientWriter) cannot allocate SHM block");
  }
  buffer_size_ = size;
  handled_timeslice_callbacks_.set_worker(make_shared<WorkerThread>());

  ts_completions_thread_ = std::async([this]() {
    while (true) {
      uint64_t found_completions = 0;
      {
        unique_lock<mutex> l(mtx_);
        while (auto c = ts_buffer_->try_receive_completion()) {
          component_ids_done_.push(c->user_data);
          found_completions++;
        }
      }

      if (found_completions != 0) {
        ts_input_output_cnt_diff_ -= found_completions;
        L_(debug) << "Available timeslices: " << ts_input_output_cnt_diff_;
        handled_timeslice_callbacks_.call_async(found_completions);
      } else {
        L_(trace) << "no completions found ...";
        this_thread::sleep_for(chrono::milliseconds(500));
      }
    }
  });
}

bool TsclientWriter::on_timeslices_handled(std::function<void(uint64_t)> cb) {
  return handled_timeslice_callbacks_.add(cb);
}

uint64_t TsclientWriter::get_buffer_size() {
  return buffer_size_;
}

std::shared_ptr<char> TsclientWriter::get_buffer() {
  return shared_ptr<char>(reinterpret_cast<char*>(buffer_), no_del(char));
}

void TsclientWriter::set_buffer_map(std::shared_ptr<BufferMap> buffer_map) {
  buffer_map_ = buffer_map;
}

void TsclientWriter::write_timeslice(std::vector<BufferMap::ListElement*>& elements) {
  // Each element holds the data of one component, in component order (see TsclientReader)
  fles::tsb::StDescriptor desc;
  for (const auto* el : elements) {
    const auto* data = buffer_ + el->address;
    auto& component = desc.components.emplace_back();
    component.ms_data_offset = ts_buffer_->offset_of(data);
    component.ms_data_size = el->len;
    component.num_microslices = count_microslices(data, el->len);
    component.flags = el->tag;
  }
  // the first element contains the TS index
  const uint64_t ts_index = elements.at(0)->user_0;
  {
    unique_lock<mutex> l(mtx_);
    L_(debug) << "send_work_item - open completions: " << ts_input_output_cnt_diff_;
    ts_input_output_cnt_diff_++;
    ts_buffer_->send_work_item(ts_index, desc, elements[0]->compontent_id);
  }
}

bool TsclientWriter::pop_finished_component_id(uint64_t& component_id) {
  unique_lock<mutex> l(mtx_);

  if (!component_ids_done_.empty()) {
    component_id = component_ids_done_.front();
    // L_(debug) << "TsclientWriter::pop_finished_component_id - component_id: " << component_id;
    component_ids_done_.pop();
    return true;
  }

  return false;
}

uint64_t TsclientWriter::get_finished_component_id_cnt() {
  unique_lock<mutex> l(mtx_);
  return component_ids_done_.size();
}
