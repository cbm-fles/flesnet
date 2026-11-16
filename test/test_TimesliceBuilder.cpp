// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
#define BOOST_TEST_MODULE test_TimesliceBuilder
#include <boost/test/unit_test.hpp>

#include "MicrosliceOutputArchive.hpp"
#include "StorableMicroslice.hpp"
#include "TimesliceAutoSource.hpp"
#include "TimesliceBuilder.hpp"
#include "TimesliceFlags.hpp"
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {

constexpr uint64_t ms = 1000000; // 1 ms in ns

fles::MicrosliceDescriptor
descriptor(uint16_t eq_id, uint64_t idx, uint32_t size, uint16_t flags = 0) {
  fles::MicrosliceDescriptor d = fles::MicrosliceDescriptor();
  d.hdr_id = static_cast<uint8_t>(fles::HeaderFormatIdentifier::Standard);
  d.hdr_ver = static_cast<uint8_t>(fles::HeaderFormatVersion::Standard);
  d.sys_id = static_cast<uint8_t>(fles::Subsystem::FLES);
  d.sys_ver = static_cast<uint8_t>(fles::SubsystemFormatFLES::Uninitialized);
  d.eq_id = eq_id;
  d.flags = flags;
  d.idx = idx;
  d.size = size;
  return d;
}

/// A microslice source reading from a vector
class VectorSource : public fles::MicrosliceSource {
public:
  explicit VectorSource(std::vector<fles::StorableMicroslice> items)
      : items_(std::move(items)) {}
  [[nodiscard]] bool eos() const override { return next_ >= items_.size(); }

private:
  fles::Microslice* do_get() override {
    if (eos()) {
      return nullptr;
    }
    return new fles::StorableMicroslice(items_[next_++]); // NOLINT
  }
  std::vector<fles::StorableMicroslice> items_;
  size_t next_ = 0;
};

/// Microslices every `step` from `first` to `last` (inclusive), the content
/// of each holding its eq_id and start time
std::vector<fles::StorableMicroslice> stream(uint16_t eq_id,
                                             uint64_t first,
                                             uint64_t last,
                                             uint64_t step = ms,
                                             uint16_t flags_at = 0) {
  std::vector<fles::StorableMicroslice> v;
  for (uint64_t idx = first; idx <= last; idx += step) {
    std::vector<uint8_t> content(16);
    std::memcpy(content.data(), &eq_id, sizeof(eq_id));
    std::memcpy(content.data() + 8, &idx, sizeof(idx));
    uint16_t flags = 0;
    if (flags_at != 0 && idx == flags_at * ms) {
      flags = static_cast<uint16_t>(fles::MicrosliceFlags::OverflowFlim);
    }
    v.emplace_back(descriptor(eq_id, idx, 16, flags), std::move(content));
  }
  return v;
}

std::vector<std::unique_ptr<fles::MicrosliceSource>>
sources(std::vector<std::vector<fles::StorableMicroslice>> streams) {
  std::vector<std::unique_ptr<fles::MicrosliceSource>> v;
  for (auto& s : streams) {
    v.push_back(std::make_unique<VectorSource>(std::move(s)));
  }
  return v;
}

fles::TimesliceBuilderParameters parameters(uint64_t duration = 10 * ms,
                                            uint64_t overlap = 100000) {
  fles::TimesliceBuilderParameters p;
  p.duration_ns = duration;
  p.overlap_before_ns = overlap;
  p.overlap_after_ns = overlap;
  return p;
}

} // namespace

BOOST_AUTO_TEST_CASE(complete_streams_test) {
  // two streams of 1 ms microslices from 0 to 99 ms, 10 ms timeslices
  fles::TimesliceBuilder builder(
      sources({stream(1, 0, 99 * ms), stream(2, 0, 99 * ms)}), parameters());

  std::vector<std::unique_ptr<fles::StorableTimeslice>> timeslices;
  while (auto ts = builder.get()) {
    timeslices.push_back(std::move(ts));
  }
  BOOST_CHECK(builder.eos());

  // The first timeslice [0, 10 ms) would need a microslice before 0 for its
  // overlap, the last [90, 100 ms) a microslice beyond 100.1 ms to be
  // complete, so the timeslices 1 to 8 are built
  BOOST_REQUIRE_EQUAL(timeslices.size(), 8);
  for (size_t i = 0; i < timeslices.size(); ++i) {
    const auto& ts = *timeslices[i];
    const uint64_t start = (i + 1) * 10 * ms;
    BOOST_CHECK_EQUAL(ts.index(), i + 1);
    BOOST_CHECK_EQUAL(ts.start_time(), start);
    BOOST_CHECK_EQUAL(ts.duration(), 10 * ms);
    BOOST_CHECK_EQUAL(ts.flags(), 0);
    BOOST_REQUIRE_EQUAL(ts.num_components(), 2);
    for (uint64_t c = 0; c < 2; ++c) {
      // the microslice covering the overlap before, the ten of the
      // timeslice, and the one covering the overlap after
      BOOST_REQUIRE_EQUAL(ts.num_microslices(c), 12);
      BOOST_CHECK_EQUAL(ts.descriptor(c, 0).idx, start - ms);
      BOOST_CHECK_EQUAL(ts.descriptor(c, 11).idx, start + 10 * ms);
      BOOST_CHECK_EQUAL(ts.descriptor(c, 0).eq_id, c + 1);
      // the content is intact
      uint64_t idx = 0;
      std::memcpy(&idx, ts.content(c, 5) + 8, sizeof(idx));
      BOOST_CHECK_EQUAL(idx, ts.descriptor(c, 5).idx);
    }
  }
}

BOOST_AUTO_TEST_CASE(overlap_flags_test) {
  // an overflow flag in one microslice marks its component and timeslice
  fles::TimesliceBuilder builder(
      sources({stream(1, 0, 99 * ms, ms, 25), stream(2, 0, 99 * ms)}),
      parameters());
  auto ts1 = builder.get();
  auto ts2 = builder.get();
  BOOST_REQUIRE(ts1 && ts2);
  BOOST_CHECK_EQUAL(ts1->flags(), 0);
  // the microslice at 25 ms is in timeslice 2 [20, 30 ms)
  BOOST_CHECK_EQUAL(ts2->flags(),
                    static_cast<uint32_t>(fles::tsb::TsFlag::OverflowFlim));
  BOOST_CHECK_EQUAL(
      ts2->component_flags(0),
      static_cast<uint32_t>(fles::tsb::TsComponentFlag::OverflowFlim));
}

BOOST_AUTO_TEST_CASE(late_source_test) {
  // the second stream begins at 50 ms: by default the first timeslice is the
  // first one both can provide
  {
    fles::TimesliceBuilder builder(
        sources({stream(1, 0, 99 * ms), stream(2, 50 * ms, 99 * ms)}),
        parameters());
    auto ts = builder.get();
    BOOST_REQUIRE(ts);
    BOOST_CHECK_EQUAL(ts->index(), 6);
    BOOST_CHECK_EQUAL(ts->num_components(), 2);
  }
  // with an explicit start time, the timeslices before are built without the
  // second component and flagged
  {
    auto p = parameters();
    p.start_time_ns = 0;
    fles::TimesliceBuilder builder(
        sources({stream(1, 0, 99 * ms), stream(2, 50 * ms, 99 * ms)}), p);
    // the window of timeslice 0 cannot begin before time zero, so the first
    // stream provides it; the second stream is missing up to timeslice 5
    auto ts = builder.get();
    BOOST_REQUIRE(ts);
    BOOST_CHECK_EQUAL(ts->index(), 0);
    BOOST_CHECK_EQUAL(ts->num_components(), 1);
    BOOST_CHECK_EQUAL(ts->descriptor(0, 0).eq_id, 1);
    BOOST_CHECK_EQUAL(ts->flags(), static_cast<uint32_t>(
                                       fles::tsb::TsFlag::MissingComponents));
    for (int i = 1; i < 6; ++i) {
      ts = builder.get();
      BOOST_REQUIRE(ts);
      BOOST_CHECK_EQUAL(ts->index(), i);
      BOOST_CHECK_EQUAL(ts->num_components(), 1);
    }
    ts = builder.get();
    BOOST_REQUIRE(ts);
    BOOST_CHECK_EQUAL(ts->index(), 6);
    BOOST_CHECK_EQUAL(ts->num_components(), 2);
    BOOST_CHECK_EQUAL(ts->flags(), 0);
  }
}

BOOST_AUTO_TEST_CASE(early_end_test) {
  // the second stream ends at 49 ms: the later timeslices lack its component
  fles::TimesliceBuilder builder(
      sources({stream(1, 0, 99 * ms), stream(2, 0, 49 * ms)}), parameters());
  size_t count = 0;
  while (auto ts = builder.get()) {
    ++count;
    if (ts->index() <= 3) {
      BOOST_CHECK_EQUAL(ts->num_components(), 2);
      BOOST_CHECK_EQUAL(ts->flags(), 0);
    } else {
      BOOST_CHECK_EQUAL(ts->num_components(), 1);
      BOOST_CHECK_EQUAL(ts->flags(), static_cast<uint32_t>(
                                         fles::tsb::TsFlag::MissingComponents));
    }
  }
  BOOST_CHECK_EQUAL(count, 8);
}

BOOST_AUTO_TEST_CASE(skip_ahead_test) {
  // a start time far before the data: the builder jumps to the data instead
  // of producing empty timeslices
  auto p = parameters();
  p.start_time_ns = 0;
  fles::TimesliceBuilder builder(sources({stream(1, 100000 * ms, 100099 * ms)}),
                                 p);
  auto ts = builder.get();
  BOOST_REQUIRE(ts);
  BOOST_CHECK_EQUAL(ts->index(), 10001);
  BOOST_CHECK_EQUAL(ts->num_components(), 1);
}

BOOST_AUTO_TEST_CASE(empty_sources_test) {
  fles::TimesliceBuilder builder(sources({{}, {}}), parameters());
  BOOST_CHECK(!builder.get());
  BOOST_CHECK(builder.eos());
  BOOST_CHECK_THROW(fles::TimesliceBuilder(sources({}), parameters()),
                    std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(auto_source_test) {
  // two microslice archives, built through the msa scheme
  {
    fles::MicrosliceOutputArchive a("test_tb_a.msa");
    for (auto& m : stream(1, 0, 99 * ms)) {
      a.put(std::make_shared<fles::StorableMicroslice>(m));
    }
    fles::MicrosliceOutputArchive b("test_tb_b.msa");
    for (auto& m : stream(2, 0, 99 * ms)) {
      b.put(std::make_shared<fles::StorableMicroslice>(m));
    }
  }
  fles::TimesliceAutoSource source(
      "msa://./test_tb_*.msa?duration=20ms&overlap=1ms&start=20000000");
  size_t count = 0;
  while (auto ts = source.get()) {
    BOOST_CHECK_EQUAL(ts->duration(), 20 * ms);
    BOOST_CHECK_EQUAL(ts->index(), count + 1);
    BOOST_CHECK_EQUAL(ts->num_components(), 2);
    // overlap of 1 ms: 1 + 20 + 1 microslices
    BOOST_CHECK_EQUAL(ts->num_microslices(0), 22);
    ++count;
  }
  // [20, 40), [40, 60), [60, 80); [80, 100) needs data beyond 101 ms
  BOOST_CHECK_EQUAL(count, 3);
  std::remove("test_tb_a.msa");
  std::remove("test_tb_b.msa");

  BOOST_CHECK_THROW(fles::TimesliceAutoSource("msa://./none_*.msa"),
                    std::runtime_error);
  BOOST_CHECK_THROW(fles::TimesliceAutoSource("msa://./*.tsa?cycles=2"),
                    std::runtime_error);
}
