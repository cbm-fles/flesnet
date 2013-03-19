/**
 * \file concurrent_queue.hpp
 *
 * 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef CONCURRENT_QUEUE_HPP
#define CONCURRENT_QUEUE_HPP

#include <boost/thread.hpp>
#include <queue>


template<typename T>
class concurrent_queue : private boost::noncopyable
{
public:
    concurrent_queue(size_t max_size = 1000) :
        _max_size(max_size)
    { }

    struct Stopped { };

    size_t max_size() const
    {
        return _max_size;
    }

    bool empty() const
    {
        boost::unique_lock<boost::mutex> lock(_mutex);
        if (_is_stopped)
            throw Stopped();
        return _queue.empty();
    }

    size_t size() const
    {
        boost::unique_lock<boost::mutex> lock(_mutex);
        if (_is_stopped)
            throw Stopped();
        return _queue.size();
    }

    bool stopped() const
    {
        boost::unique_lock<boost::mutex> lock(_mutex);
        return _is_stopped;
    }
    
    void push(const T& data)
    {
        {
            boost::unique_lock<boost::mutex> lock(_mutex);
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
            boost::mutex::scoped_lock lock(_mutex);
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
            boost::unique_lock<boost::mutex> lock(_mutex);
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
            boost::mutex::scoped_lock lock(_mutex);
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
        boost::unique_lock<boost::mutex> lock(_mutex);
        while (!_queue.empty() && !_is_stopped) {
            _cond_empty.wait(lock);
        }
        if (_is_stopped)
            throw Stopped();
    }
    
private:
    std::queue<T> _queue;
    const size_t _max_size;
    mutable boost::mutex _mutex;
    boost::condition_variable _cond_not_empty;
    boost::condition_variable _cond_not_full;
    mutable boost::condition_variable _cond_empty;
    bool _is_stopped = false;
};


#endif /* CONCURRENT_QUEUE_HPP */
