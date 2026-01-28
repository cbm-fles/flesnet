// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::AggregatingSource template class.
#pragma once

#include "log.hpp"
#include <algorithm>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace fles {

/**
 * \brief The AggregatingSource class merges data sets from a given set of input
 * sources without any respect to order.
 *
 * This class is meant to be used for special cases, not for regular online
 * operation.
 */
template <class SourceType> class AggregatingSource : public SourceType {
public:
  using item_type = typename SourceType::item_type;

  /**
   * \brief Construct an aggregating source object, initialize the list of input
   * sources, and start peeking into the item streams
   *
   * \param sources The input sources to read data from
   */
  AggregatingSource(std::vector<std::unique_ptr<SourceType>> sources) {
    if (sources.empty()) {
      eos_ = true;
    }

    for (auto& source : sources) {
      async_sources_.emplace_back(std::make_unique<AsyncSource>(
          std::move(source), *this, async_sources_.size()));
    }
  }

  /// Delete copy constructor (non-copyable).
  AggregatingSource(const AggregatingSource&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const AggregatingSource&) = delete;

  ~AggregatingSource() override {
    // Request all threads to stop
    for (auto& async_source : async_sources_) {
      async_source->prefetch_thread.request_stop();
    }
    // Wake up all threads so they can check the stop token
    for (auto& async_source : async_sources_) {
      std::lock_guard<std::mutex> lock(async_source->mutex);
      async_source->item_consumed = true;
      async_source->cv_consumed.notify_one();
    }
  }

  [[nodiscard]] bool eos() const override { return eos_; }

private:
  struct AsyncSource {
    std::unique_ptr<SourceType> source;
    std::unique_ptr<item_type> prefetched_item = nullptr;
    std::jthread prefetch_thread;
    std::size_t index;
    std::size_t items_fetched = 0;

    // Synchronization primitives
    std::mutex mutex;
    std::condition_variable cv_consumed; // signaled when item was consumed
    bool item_consumed = true;     // true when main thread has taken the item
    bool source_exhausted = false; // true when source returned nullptr

    AggregatingSource& parent;

    AsyncSource(std::unique_ptr<SourceType> src,
                AggregatingSource& parent,
                std::size_t index)
        : source(std::move(src)), index(index), parent(parent) {
      prefetch_thread =
          std::jthread([this](std::stop_token st) { thread_loop(st); });
    }

    void thread_loop(const std::stop_token& st) {
      while (!st.stop_requested()) {
        // Fetch the next item (this may block)
        L_(debug) << "AsyncSource " << index << ": fetching item "
                  << items_fetched << " from source";
        auto item = source->get();

        std::unique_lock<std::mutex> lock(mutex);

        if (item == nullptr) {
          // Source is exhausted
          L_(debug) << "AsyncSource " << index << ": source exhausted";
          source_exhausted = true;
          prefetched_item = nullptr;
          parent.cv_any_available_.notify_one();
          break;
        }

        // Store the prefetched item and signal readiness
        prefetched_item = std::move(item);
        L_(debug) << "AsyncSource " << index << ": item " << items_fetched
                  << " prefetched";
        items_fetched++;
        item_consumed = false;
        parent.cv_any_available_.notify_one();

        // Wait until the item has been consumed (or stop requested)
        cv_consumed.wait(
            lock, [this, &st] { return item_consumed || st.stop_requested(); });
      }
    }

    /// Check if an item is available (call with mutex held)
    [[nodiscard]] bool has_item_available() const {
      return prefetched_item != nullptr;
    }

    /// Check if the source is exhausted (call with mutex held)
    [[nodiscard]] bool is_exhausted() const { return source_exhausted; }

    /// Take the prefetched item (call with mutex held)
    std::unique_ptr<item_type> take_item() {
      item_consumed = true;
      cv_consumed.notify_one();
      return std::move(prefetched_item);
    }
  };

  std::vector<std::unique_ptr<AsyncSource>> async_sources_;
  std::size_t next_source_index_ = 0;

  // Synchronization for do_get() blocking
  std::mutex main_mutex_;
  std::condition_variable cv_any_available_;

  bool eos_ = false;

  item_type* do_get() override {
    if (eos_) {
      return nullptr;
    }

    std::unique_lock<std::mutex> main_lock(main_mutex_);

    L_(debug) << "AggregatingSource: do_get() called";
    while (true) {
      // Loop over all sources, starting from next_source_index_, to find the
      // next available item
      bool has_active_sources = false;
      for (std::size_t i = 0; i < async_sources_.size(); ++i) {
        std::size_t source_index =
            (next_source_index_ + i) % async_sources_.size();
        auto& async_source = async_sources_.at(source_index);

        std::unique_lock<std::mutex> source_lock(async_source->mutex);
        if (async_source->has_item_available()) {
          L_(debug) << "AggregatingSource: item available from source "
                    << source_index;
          auto item = async_source->take_item();
          next_source_index_ = (source_index + 1) % async_sources_.size();
          return item.release();
        }
        L_(debug) << "AggregatingSource: no item available from source "
                  << source_index;
        if (!async_source->is_exhausted()) {
          has_active_sources = true;
        }
      }

      // No items available - check if all sources are exhausted
      if (!has_active_sources) {
        eos_ = true;
        return nullptr;
      }
      L_(debug) << "AggregatingSource: no items available yet, waiting...";

      // Wait for an item to become available or all sources to be exhausted
      cv_any_available_.wait(main_lock);
    }
  }
};

} // namespace fles
