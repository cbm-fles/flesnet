// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceBuilder class.
#pragma once

#include "ComponentWindow.hpp"
#include "Microslice.hpp"
#include "MicrosliceSource.hpp"
#include "StorableTimeslice.hpp"
#include "TimesliceSource.hpp"
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <vector>

namespace fles {

/// The parameters of a TimesliceBuilder.
struct TimesliceBuilderParameters {
  /// The duration of a timeslice in nanoseconds
  uint64_t duration_ns = 40000000;
  /// The overlap before the start of a timeslice in nanoseconds
  uint64_t overlap_before_ns = 100000;
  /// The overlap after the end of a timeslice in nanoseconds
  uint64_t overlap_after_ns = 100000;
  /// The start time of the first timeslice (default: the first timeslice that
  /// every source can provide completely)
  std::optional<uint64_t> start_time_ns;
};

/**
 * \brief The TimesliceBuilder class builds timeslices from microslice sources.
 *
 * Each source is one input channel and provides one component of every
 * timeslice. The timeslices are cut by time exactly like the online timeslice
 * building does (see ComponentWindow.hpp): a timeslice [start, start +
 * duration) gets every microslice that starts within the overlaps around it,
 * and a component is complete once a microslice beyond the window has been
 * read from its source.
 *
 * A component that a source cannot provide, because its microslices start
 * after the window or because the stream has ended before the window end, is
 * left out and the timeslice is flagged (TsFlag::MissingComponents). A
 * timeslice that no source can contribute to is skipped. The stream ends when
 * no source can contribute to any further timeslice.
 *
 * This is the reference for building timeslices from recorded or simulated
 * microslice streams in a single process, without the timeslice building
 * network and without the data clock.
 */
class TimesliceBuilder : public TimesliceSource {
public:
  TimesliceBuilder(std::vector<std::unique_ptr<MicrosliceSource>> sources,
                   TimesliceBuilderParameters parameters);

  /// Delete copy constructor (non-copyable).
  TimesliceBuilder(const TimesliceBuilder&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const TimesliceBuilder&) = delete;

  ~TimesliceBuilder() override = default;

  /// Read the next timeslice.
  std::unique_ptr<StorableTimeslice> get() {
    return std::unique_ptr<StorableTimeslice>(do_get());
  };

  [[nodiscard]] bool eos() const override { return eos_; }

private:
  struct Component {
    std::unique_ptr<MicrosliceSource> source;
    /// The microslices read from the source and not yet released, by time
    std::deque<std::unique_ptr<Microslice>> buffer;
    /// Whether the source has been read to its end
    bool exhausted = false;
    ComponentAvailability state = ComponentAvailability::TryLater;

    /// Read one more microslice from the source into the buffer
    bool fetch();
  };

  StorableTimeslice* do_get() override;

  /// Make every source provide its state for the given window, reading from
  /// the sources as needed
  void update_states(const ComponentWindow& window);
  /// The start time of the first timeslice
  [[nodiscard]] std::optional<uint64_t> initial_start_time();
  /// The earliest start time after the current one that some source can
  /// provide, if any
  [[nodiscard]] std::optional<uint64_t> next_possible_start_time() const;
  /// Build the timeslice of the current window from the available components
  [[nodiscard]] StorableTimeslice* build(const ComponentWindow& window);
  /// Release the microslices no longer needed once the timeslice starting at
  /// the given time has been built
  void release_before(uint64_t start_time_ns);

  [[nodiscard]] uint64_t round_up(uint64_t time_ns) const {
    return (time_ns + parameters_.duration_ns - 1) / parameters_.duration_ns *
           parameters_.duration_ns;
  }

  std::vector<Component> components_;
  TimesliceBuilderParameters parameters_;
  std::optional<uint64_t> start_time_ns_;
  bool eos_ = false;
};

} // namespace fles
