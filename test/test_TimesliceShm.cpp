/* Copyright (C) 2026 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */
#define BOOST_TEST_MODULE test_TimesliceShm
#include <boost/test/unit_test.hpp>

#include "StorableTimeslice.hpp"
#include "System.hpp"
#include "TimesliceInputArchive.hpp"
#include "TimesliceReceiver.hpp"
#include "TimesliceShmBuffer.hpp"
#include "TimesliceShmSink.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <zmq.hpp>

namespace {

WorkerParameters worker_parameters() {
  return {1, 0, WorkerQueuePolicy::QueueAll, 0, "test_TimesliceShm"};
}

// Receive a single timeslice, giving the worker time to register before the
// item is sent by the given function
std::unique_ptr<fles::TimesliceView>
receive_one(fles::Receiver<fles::Timeslice, fles::TimesliceView>& receiver,
            const std::function<void()>& send) {
  auto future = std::async(std::launch::async, [&] { return receiver.get(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  send();
  BOOST_REQUIRE(future.wait_for(std::chrono::seconds(5)) ==
                std::future_status::ready);
  return future.get();
}

void check_equal(const fles::Timeslice& a, const fles::Timeslice& b) {
  BOOST_CHECK_EQUAL(a.index(), b.index());
  BOOST_CHECK_EQUAL(a.start_time(), b.start_time());
  BOOST_REQUIRE_EQUAL(a.num_components(), b.num_components());
  for (uint64_t c = 0; c < a.num_components(); ++c) {
    BOOST_CHECK_EQUAL(a.num_microslices(c), b.num_microslices(c));
    BOOST_REQUIRE_EQUAL(a.size_component(c), b.size_component(c));
    for (uint64_t m = 0; m < a.num_microslices(c); ++m) {
      BOOST_REQUIRE_EQUAL(a.descriptor(c, m).size, b.descriptor(c, m).size);
      BOOST_CHECK(std::memcmp(a.content(c, m), b.content(c, m),
                              a.descriptor(c, m).size) == 0);
    }
  }
}

} // namespace

// Pass timeslices through shared memory twice, copying the data block between
// the segments like a forwarder would
BOOST_AUTO_TEST_CASE(shm_forward_test) {
  zmq::context_t context;
  const std::string pid = std::to_string(fles::system::current_pid());
  const std::string in_id = "test_TimesliceShm_in_" + pid;
  const std::string out_id = "test_TimesliceShm_out_" + pid;

  fles::TimesliceShmSink sink(context, in_id, UINT64_C(1) << 24);
  fles::TimesliceShmBuffer out_buffer(context, out_id, UINT64_C(1) << 24);

  fles::Receiver<fles::Timeslice, fles::TimesliceView> in_receiver(
      in_id, worker_parameters(), fles::ShmAccess::ReadWrite);
  fles::Receiver<fles::Timeslice, fles::TimesliceView> out_receiver(
      out_id, worker_parameters());

  fles::TimesliceInputArchive archive("example1.tsa");
  uint64_t count = 0;
  while (auto original = archive.get()) {
    std::shared_ptr<const fles::Timeslice> ts = std::move(original);

    auto view = receive_one(in_receiver, [&] { sink.put(ts); });
    BOOST_REQUIRE(view);
    check_equal(*ts, *view);

    const auto region = view->shm_region();
    fles::tsb::StDescriptor desc = view->st_descriptor();
    BOOST_REQUIRE_EQUAL(desc.components.size(), view->num_components());

    // Copy the components one by one, each into an allocation of its own
    for (uint64_t c = 0; c < view->num_components(); ++c) {
      const auto data = view->component_data(c);
      BOOST_CHECK(data.data() >= region.data());
      BOOST_CHECK(data.data() + data.size() <= region.data() + region.size());
      BOOST_CHECK_EQUAL(desc.components[c].ms_data_offset,
                        data.data() - region.data());
      BOOST_CHECK_EQUAL(desc.components[c].ms_data_size, data.size());

      std::byte* out_data =
          out_buffer.allocate(std::max<size_t>(data.size(), 1));
      BOOST_REQUIRE(out_data != nullptr);
      std::memcpy(out_data, data.data(), data.size());
      desc.components[c].ms_data_offset = out_buffer.offset_of(out_data);
    }

    auto forwarded = receive_one(
        out_receiver, [&] { out_buffer.send_work_item(view->index(), desc); });
    BOOST_REQUIRE(forwarded);
    check_equal(*ts, *forwarded);
    BOOST_CHECK(forwarded->shm_uuid() != view->shm_uuid());

    forwarded.reset();
    view.reset();
    ++count;
  }
  BOOST_CHECK(count > 0);
}
