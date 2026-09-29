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
#if __cplusplus >= 202002L
#include <stop_token>
#else
#include <atomic>
#endif

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
    {
      std::lock_guard<std::mutex> lock(mutex_);
      for (auto& async_source : async_sources_) {
#if __cplusplus >= 202002L
        async_source->prefetch_thread.request_stop();
#else
        async_source->stop_requested = true;
#endif
        // Wake up the thread so it can check the stop condition
        async_source->cv_consumed.notify_one();
      }
    }
    // Join all threads while the synchronization primitives are still alive
    for (auto& async_source : async_sources_) {
      if (async_source->prefetch_thread.joinable()) {
        async_source->prefetch_thread.join();
      }
    }
  }

  [[nodiscard]] bool eos() const override { return eos_; }

private:
  struct AsyncSource {
    std::unique_ptr<SourceType> source;
    std::size_t index;
    std::size_t items_fetched = 0;
    AggregatingSource& parent;

    // State shared with the main thread, guarded by parent.mutex_
    std::unique_ptr<item_type> prefetched_item = nullptr;
    bool source_exhausted = false;
    std::condition_variable cv_consumed; // signaled when item was consumed
#if __cplusplus >= 202002L
    std::jthread prefetch_thread;
#else
    std::atomic<bool> stop_requested{false};
    std::thread prefetch_thread;
#endif

    AsyncSource(std::unique_ptr<SourceType> src,
                AggregatingSource& parent,
                std::size_t index)
        : source(std::move(src)), index(index), parent(parent) {
#if __cplusplus >= 202002L
      prefetch_thread =
          std::jthread([this](std::stop_token st) { thread_loop(st); });
#else
      prefetch_thread = std::thread([this]() { thread_loop(); });
#endif
    }

#if __cplusplus >= 202002L
    void thread_loop(const std::stop_token& st) {
      auto stop = [&st] { return st.stop_requested(); };
#else
    void thread_loop() {
      auto stop = [this] { return stop_requested.load(); };
#endif
      while (!stop()) {
        // Fetch the next item (this may block)
        L_(debug) << "AsyncSource " << index << ": fetching item "
                  << items_fetched << " from source";
        auto item = source->get();

        // All state changes that the main thread waits for happen under
        // parent.mutex_, so that no notification can get lost
        std::unique_lock<std::mutex> lock(parent.mutex_);

        if (item == nullptr) {
          L_(debug) << "AsyncSource " << index << ": source exhausted";
          source_exhausted = true;
          parent.cv_any_available_.notify_one();
          break;
        }

        // Store the prefetched item and signal readiness
        prefetched_item = std::move(item);
        L_(debug) << "AsyncSource " << index << ": item " << items_fetched
                  << " prefetched";
        items_fetched++;
        parent.cv_any_available_.notify_one();

        // Wait until the item has been consumed (or stop requested)
        cv_consumed.wait(lock, [this, &stop] {
          return prefetched_item == nullptr || stop();
        });
      }
    }
  };

  // Guards the shared state of all AsyncSource objects; declared before
  // async_sources_ so that it outlives them
  std::mutex mutex_;
  std::condition_variable cv_any_available_;

  std::vector<std::unique_ptr<AsyncSource>> async_sources_;
  std::size_t next_source_index_ = 0;

  bool eos_ = false;

  item_type* do_get() override {
    if (eos_) {
      return nullptr;
    }

    std::unique_lock<std::mutex> lock(mutex_);

    L_(debug) << "AggregatingSource: do_get() called";
    while (true) {
      // Loop over all sources, starting from next_source_index_, to find the
      // next available item
      bool has_active_sources = false;
      for (std::size_t i = 0; i < async_sources_.size(); ++i) {
        std::size_t source_index =
            (next_source_index_ + i) % async_sources_.size();
        auto& async_source = async_sources_.at(source_index);

        if (async_source->prefetched_item != nullptr) {
          L_(debug) << "AggregatingSource: item available from source "
                    << source_index;
          auto item = std::move(async_source->prefetched_item);
          async_source->cv_consumed.notify_one();
          next_source_index_ = (source_index + 1) % async_sources_.size();
          return item.release();
        }
        L_(debug) << "AggregatingSource: no item available from source "
                  << source_index;
        if (!async_source->source_exhausted) {
          has_active_sources = true;
        }
      }

      // No items available - check if all sources are exhausted
      if (!has_active_sources) {
        eos_ = true;
        return nullptr;
      }
      L_(debug) << "AggregatingSource: no items available yet, waiting...";

      // Wait for an item to become available or a source to be exhausted
      cv_any_available_.wait(lock);
    }
  }
};

} // namespace fles
