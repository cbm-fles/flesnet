#pragma once
/**
 * \file concurrent_queue.hpp
 *
 * 2013, Jan de Cuveland <cmail@cuveland.de>
 */

template <typename T>
class concurrent_queue
{
public:
    struct Stopped
    {
    };

    explicit concurrent_queue(size_t new_max_size = 1000)
        : _max_size(new_max_size)
    {
    }

    concurrent_queue(const concurrent_queue&) = delete;
    void operator=(const concurrent_queue&) = delete;

    size_t max_size() const
    {
        return _max_size;
    }

    bool empty() const
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_is_stopped)
            throw Stopped();
        return _queue.empty();
    }

    size_t size() const
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_is_stopped)
            throw Stopped();
        return _queue.size();
    }

    bool stopped() const
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _is_stopped;
    }

    void push(const T& data)
    {
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_is_stopped)
                throw Stopped();
            while (_queue.size() == _max_size) {
                _cond_not_full.wait(lock);
            }
            _queue.push(data);
        }
        _cond_not_empty.notify_one();
    }

    bool try_pop(T& data)
    {
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_is_stopped)
                throw Stopped();
            if (_queue.empty())
                return false;
            data = _queue.front();
            _queue.pop();
            if (_queue.empty())
                _cond_empty.notify_all();
        }
        _cond_not_full.notify_one();

        return true;
    }

    void wait_and_pop(T& data)
    {
        {
            std::unique_lock<std::mutex> lock(_mutex);
            while (_queue.empty() && !_is_stopped) {
                _cond_not_empty.wait(lock);
            }
            if (_is_stopped)
                throw Stopped();
            data = _queue.front();
            _queue.pop();
            if (_queue.empty())
                _cond_empty.notify_all();
        }
        _cond_not_full.notify_one();
    }

    void stop()
    {
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_is_stopped)
                throw Stopped();
            _is_stopped = true;
        }
        _cond_not_empty.notify_all();
        _cond_not_full.notify_all();
        _cond_empty.notify_all();
    }

    void wait_until_empty() const
    {
        std::unique_lock<std::mutex> lock(_mutex);
        while (!_queue.empty() && !_is_stopped) {
            _cond_empty.wait(lock);
        }
        if (_is_stopped)
            throw Stopped();
    }

private:
    std::queue<T> _queue;
    const size_t _max_size;
    mutable std::mutex _mutex;
    std::condition_variable _cond_not_empty;
    std::condition_variable _cond_not_full;
    mutable std::condition_variable _cond_empty;
    bool _is_stopped = false;
};
