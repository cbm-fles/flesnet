// Copyright 2012-2013, 2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <chrono>
#include <ctime>
#include <functional>
#include <queue>
#include <utility>

class Scheduler {
public:
  struct event {
    using callback_type = std::function<void()>;
    using time_type = std::chrono::time_point<std::chrono::system_clock>;

    event(callback_type cb, const time_type& when)
        : callback_(std::move(cb)), when_(when) {}

    void operator()() const { callback_(); }

    callback_type callback_;
    time_type when_;
  };

  struct event_less : public std::less<> {
    bool operator()(const event& e1, const event& e2) const {
      return (e2.when_ < e1.when_);
    }
  };

  void add(const event::callback_type& cb, const time_t& when) {
    auto real_when = std::chrono::system_clock::from_time_t(when);

    event_queue_.emplace(cb, real_when);
  }

  void add(const event::callback_type& cb, const timeval& when) {
    auto real_when = std::chrono::system_clock::from_time_t(when.tv_sec) +
                     std::chrono::microseconds(when.tv_usec);

    event_queue_.emplace(cb, real_when);
  }

  void add(const event::callback_type& cb,
           const std::chrono::time_point<std::chrono::system_clock>& when) {
    event_queue_.emplace(cb, when);
  }

  void timer() {
    event::time_type now = std::chrono::system_clock::now();

    while (!event_queue_.empty() && (event_queue_.top().when_ <= now)) {
      event_queue_.top()();
      event_queue_.pop();
    }
  }

  [[nodiscard]] bool empty() const { return event_queue_.empty(); }

  [[nodiscard]] event::time_type when_next() const {
    if (event_queue_.empty()) {
      return std::chrono::time_point<std::chrono::system_clock>::max();
    }
    return event_queue_.top().when_;
  }

private:
  std::priority_queue<event, std::vector<event>, event_less> event_queue_;
};
