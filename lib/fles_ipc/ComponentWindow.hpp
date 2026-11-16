// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Selecting the microslices of a timeslice component from a
/// time-ordered microslice sequence.
#pragma once

#include "MicrosliceDescriptor.hpp"
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace fles {

/**
 * \brief The time range of the microslices of a timeslice component.
 *
 * The component of a timeslice [start, start + duration) contains every
 * microslice that may hold data of this timeslice: the microslice that covers
 * `begin_ns = start - overlap_before`, and every following microslice that
 * starts before `end_ns = start + duration + overlap_after`.
 *
 * The functions below operate on a sequence of microslices sorted by start
 * time. They are the single definition of these rules, shared by the online
 * timeslice building in stserver and the offline TimesliceBuilder.
 */
struct ComponentWindow {
  uint64_t begin_ns; ///< start time of the timeslice minus the overlap before
  uint64_t end_ns;   ///< end time of the timeslice plus the overlap after
};

/// The component window of the timeslice [start_ns, start_ns + duration_ns).
/// The window cannot begin before time zero.
constexpr ComponentWindow component_window(uint64_t start_ns,
                                           uint64_t duration_ns,
                                           uint64_t overlap_before_ns,
                                           uint64_t overlap_after_ns) {
  return {start_ns > overlap_before_ns ? start_ns - overlap_before_ns : 0,
          start_ns + duration_ns + overlap_after_ns};
}

/// Whether a sequence of microslices can provide a component.
enum class ComponentAvailability {
  Ok,       ///< the component can be built from the sequence
  TryLater, ///< the sequence does not reach the end of the window yet
  Failed    ///< the sequence starts after the window, the component is lost
};

/// The default projection: the start time of a microslice descriptor.
struct MicrosliceIdx {
  uint64_t operator()(const MicrosliceDescriptor& d) const { return d.idx; }
};

/**
 * \brief Check whether the microslices [begin, end) can provide the component
 * for a window.
 *
 * The sequence can provide the component if it starts at or before the window
 * and reaches beyond its end: a microslice that starts after `end_ns` proves
 * that no microslice of the window is still missing.
 *
 * \param idx_of Projection from a sequence element to the microslice start time
 */
template <class Iter, class IdxOf = MicrosliceIdx>
ComponentAvailability component_availability(Iter begin,
                                             Iter end,
                                             const ComponentWindow& window,
                                             IdxOf idx_of = IdxOf()) {
  if (begin == end) {
    return ComponentAvailability::TryLater;
  }
  if (window.begin_ns < idx_of(*begin)) {
    // the oldest microslice is younger than the first one we want
    return ComponentAvailability::Failed;
  }
  if (window.end_ns >= idx_of(*std::prev(end))) {
    // the youngest microslice does not yet reach beyond the window
    return ComponentAvailability::TryLater;
  }
  return ComponentAvailability::Ok;
}

/**
 * \brief The microslices [first, last) of the component for a window.
 *
 * `first` is the last microslice starting at or before `begin_ns`, `last` is
 * the first microslice starting at or after `end_ns`. The sequence must be
 * able to provide the component (see component_availability()), otherwise
 * std::out_of_range is thrown.
 */
template <class Iter, class IdxOf = MicrosliceIdx>
std::pair<Iter, Iter> component_range(Iter begin,
                                      Iter end,
                                      const ComponentWindow& window,
                                      IdxOf idx_of = IdxOf()) {
  // the microslice before the first one starting after begin_ns
  Iter first = std::upper_bound(
      begin, end, window.begin_ns,
      [&idx_of](uint64_t t, const auto& item) { return t < idx_of(item); });
  if (first == begin || first == end) {
    throw std::out_of_range(
        "component_range: beginning of component out of range");
  }
  --first;

  // the first microslice starting at or after end_ns
  Iter last = std::lower_bound(
      first, end, window.end_ns,
      [&idx_of](const auto& item, uint64_t t) { return idx_of(item) < t; });
  if (last == begin || last == end) {
    throw std::out_of_range("component_range: end of component out of range");
  }

  return {first, last};
}

/**
 * \brief The first microslice to keep when the microslices before `time_ns`
 * are no longer needed.
 *
 * This is the last microslice starting at or before `time_ns`, as it covers
 * `time_ns` itself, or `begin` if there is none. To keep the component of a
 * timeslice starting at `start_ns` buildable, pass `start_ns -
 * overlap_before`.
 */
template <class Iter, class IdxOf = MicrosliceIdx>
Iter component_release_point(Iter begin,
                             Iter end,
                             uint64_t time_ns,
                             IdxOf idx_of = IdxOf()) {
  Iter it = std::upper_bound(
      begin, end, time_ns,
      [&idx_of](uint64_t t, const auto& item) { return t < idx_of(item); });
  if (it != begin) {
    --it;
  }
  return it;
}

} // namespace fles
