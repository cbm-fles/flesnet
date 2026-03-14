#include <memory>
#include <vector>
#include <df/BufferMap/BufferMap.hpp>

#pragma once

class TsSink {
public:
    virtual void write_timeslice(std::vector<BufferMap::ListElement*>& elements) = 0;
    // virtual void clear_last_timeslice() = 0;
    virtual std::shared_ptr<char> get_buffer() = 0;
    virtual uint64_t get_buffer_size() = 0;
    virtual void set_buffer(std::shared_ptr<char> buffer) = 0;
    virtual ~TsSink() {};
};