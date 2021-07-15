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
 * sources.
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

    auto result = std::min_element(prefetched_items_.begin(),
                                   prefetched_items_.end(), comparator);
    if (*result = nullptr) {
      eos_ = true;
      return nullptr;
    }
    auto item = result;
    result = source->get();
    return item.release();
  };
};

} // namespace fles
