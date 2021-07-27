// Copyright 2020 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::MergingSource template class.
#pragma once

/*
#include "ArchiveDescriptor.hpp"
#include "Source.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <fstream>
#include <iomanip>
*/
#include <memory>
/*
#include <sstream>
#include <string>
*/

namespace fles {

/**
 * \brief The MergingSource class merges data sets from a given set of input
 * sources. Because of the way that data is provided by the source, one item
 * from every source has to be kept in memory at all times.
 *
 * This class is meant to be used for detector debugging and similar special
 * cases, not for regular online operation.
 */
template <class Base> class MergingSource : public Source<Base> {
public:
  /**
   * \brief Construct a merging source object, initialize the list of input
   * sources, and start peeking into the item streams
   *
   * \param sources The input sources to read data from
   */
  MergingSource(std::vector<std::unique_ptr<Source<Base>>> sources)
      : sources_(sources) {}

  /// Delete copy constructor (non-copyable).
  MergingSource(const MergingSource&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const MergingSource&) = delete;

  ~MergingSource() override = default;

  bool eos() const override { return eos_; }

private:
  std::vector<std::unique_ptr<Source<Base>>> sources_;
  std::vector<std::unique_ptr<Base>> prefetched_items_;

  bool eos_ = false;

  void init_prefetch() {
    for (auto& source : sources_) {
      auto timeslice = source->get();
      prefetched_items_.push_back(timeslice);
    }
  }

  Base* do_get() override {
    if (eos_) {
      return nullptr;
    }

    auto item_it =
        std::min_element(prefetched_items_.begin(), prefetched_items_.end(),
                         [](const auto& a, const auto& b) {
                           if (!a)
                             return b;
                           if (!b)
                             return a;
                           return a->index() < b->index();
                         });
    auto item_index = item_it - prefetched_items_.begin();

    if (*item_it == nullptr) {
      eos_ = true;
      return nullptr;
    }

    auto item = item_it->release();
    prefetched_items_.at(item_index) = sources_.at(item_index)->get();
    return item;
  };
};

} // namespace fles
