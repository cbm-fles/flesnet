// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceBuilder.hpp"
#include "MicrosliceDescriptor.hpp"
#include "TimesliceFlags.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace fles {

namespace {
struct IdxOf {
  uint64_t operator()(const std::unique_ptr<Microslice>& ms) const {
    return ms->desc().idx;
  }
};
} // namespace

TimesliceBuilder::TimesliceBuilder(
    std::vector<std::unique_ptr<MicrosliceSource>> sources,
    TimesliceBuilderParameters parameters)
    : parameters_(parameters), start_time_ns_(parameters.start_time_ns) {
  if (parameters_.duration_ns == 0) {
    throw std::invalid_argument("timeslice duration must be greater than 0");
  }
  if (sources.empty()) {
    throw std::invalid_argument("no microslice sources to build from");
  }
  components_.reserve(sources.size());
  for (auto& source : sources) {
    Component component;
    component.source = std::move(source);
    components_.push_back(std::move(component));
  }
}

bool TimesliceBuilder::Component::fetch() {
  if (exhausted) {
    return false;
  }
  auto ms = source->get();
  if (!ms) {
    exhausted = true;
    return false;
  }
  if (!buffer.empty() && ms->desc().idx < buffer.back()->desc().idx) {
    throw std::runtime_error(
        "microslice start times are not monotonic in source");
  }
  buffer.push_back(std::move(ms));
  return true;
}

void TimesliceBuilder::update_states(const ComponentWindow& window) {
  for (auto& component : components_) {
    while (true) {
      component.state = component_availability(
          component.buffer.begin(), component.buffer.end(), window, IdxOf());
      if (component.state != ComponentAvailability::TryLater ||
          !component.fetch()) {
        break;
      }
    }
  }
}

std::optional<uint64_t> TimesliceBuilder::initial_start_time() {
  if (parameters_.start_time_ns) {
    return *parameters_.start_time_ns;
  }
  // The first timeslice whose window begins at or after the first microslice
  // of every source
  std::optional<uint64_t> latest_first_idx;
  for (auto& component : components_) {
    if (component.buffer.empty() && !component.fetch()) {
      continue; // an empty source cannot contribute at all
    }
    const uint64_t first_idx = component.buffer.front()->desc().idx;
    if (!latest_first_idx || first_idx > *latest_first_idx) {
      latest_first_idx = first_idx;
    }
  }
  if (!latest_first_idx) {
    return std::nullopt;
  }
  return round_up(*latest_first_idx + parameters_.overlap_before_ns);
}

std::optional<uint64_t> TimesliceBuilder::next_possible_start_time() const {
  // A source whose microslices start after the current window can contribute
  // to the first timeslice whose window begins at its first microslice
  std::optional<uint64_t> next;
  for (const auto& component : components_) {
    if (component.state != ComponentAvailability::Failed) {
      continue;
    }
    const uint64_t first_idx = component.buffer.front()->desc().idx;
    const uint64_t start = round_up(first_idx + parameters_.overlap_before_ns);
    if (!next || start < *next) {
      next = start;
    }
  }
  return next;
}

StorableTimeslice* TimesliceBuilder::do_get() {
  if (eos_) {
    return nullptr;
  }
  if (!start_time_ns_) {
    start_time_ns_ = initial_start_time();
    if (!start_time_ns_) {
      eos_ = true;
      return nullptr;
    }
  }

  while (true) {
    const ComponentWindow window = component_window(
        *start_time_ns_, parameters_.duration_ns, parameters_.overlap_before_ns,
        parameters_.overlap_after_ns);
    update_states(window);

    const bool any_available = std::any_of(
        components_.begin(), components_.end(), [](const Component& c) {
          return c.state == ComponentAvailability::Ok;
        });
    if (any_available) {
      StorableTimeslice* timeslice = build(window);
      *start_time_ns_ += parameters_.duration_ns;
      release_before(*start_time_ns_);
      return timeslice;
    }

    // Nothing for this timeslice: skip ahead to the next one some source can
    // contribute to, or end the stream if there is none
    auto next = next_possible_start_time();
    if (!next) {
      eos_ = true;
      return nullptr;
    }
    *start_time_ns_ =
        std::max(*next, *start_time_ns_ + parameters_.duration_ns);
    release_before(*start_time_ns_);
  }
}

StorableTimeslice* TimesliceBuilder::build(const ComponentWindow& window) {
  using tsb::TsComponentFlag;
  using tsb::TsFlag;

  uint32_t ts_flags = 0;
  struct Range {
    std::deque<std::unique_ptr<Microslice>>::iterator first;
    std::deque<std::unique_ptr<Microslice>>::iterator last;
    uint32_t flags;
  };
  std::vector<Range> ranges;
  ranges.reserve(components_.size());

  for (auto& component : components_) {
    if (component.state != ComponentAvailability::Ok) {
      ts_flags |= static_cast<uint32_t>(TsFlag::MissingComponents);
      continue;
    }
    auto [first, last] = component_range(
        component.buffer.begin(), component.buffer.end(), window, IdxOf());
    uint32_t flags = 0;
    for (auto it = first; it != last; ++it) {
      if (((*it)->desc().flags &
           static_cast<uint16_t>(MicrosliceFlags::OverflowFlim)) != 0) {
        flags |= static_cast<uint32_t>(TsComponentFlag::OverflowFlim);
        ts_flags |= static_cast<uint32_t>(TsFlag::OverflowFlim);
        break;
      }
    }
    ranges.push_back({first, last, flags});
  }

  auto* timeslice = new StorableTimeslice( // NOLINT
      *start_time_ns_ / parameters_.duration_ns, *start_time_ns_,
      parameters_.duration_ns, ts_flags);
  for (const auto& range : ranges) {
    const uint32_t c = timeslice->append_component(
        static_cast<uint64_t>(std::distance(range.first, range.last)), 0,
        range.flags);
    uint64_t m = 0;
    for (auto it = range.first; it != range.last; ++it, ++m) {
      timeslice->append_microslice(c, m, (*it)->desc(), (*it)->content());
    }
  }
  return timeslice;
}

void TimesliceBuilder::release_before(uint64_t start_time_ns) {
  const uint64_t keep_time = start_time_ns > parameters_.overlap_before_ns
                                 ? start_time_ns - parameters_.overlap_before_ns
                                 : 0;
  for (auto& component : components_) {
    auto keep = component_release_point(
        component.buffer.begin(), component.buffer.end(), keep_time, IdxOf());
    component.buffer.erase(component.buffer.begin(), keep);
  }
}

} // namespace fles
