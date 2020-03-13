// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <chrono>
#include <functional>
#include <queue>
#include <sys/time.h> // for `time_t` and `struct timeval`

class Scheduler {
public:
  struct event {
    using callback_type = std::function<void()>;
    using time_type = std::chrono::time_point<std::chrono::system_clock>;

    event(const callback_type& cb, const time_type& when)
        : callback_(cb), when_(when) {}

    void operator()() const { callback_(); }

    callback_type callback_;
    time_type when_;
  };

  struct event_less : public std::less<event> {
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

private:
  std::priority_queue<event, std::vector<event>, event_less> event_queue_;
};
