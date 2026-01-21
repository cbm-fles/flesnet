#pragma once

class TsSink {
public:
    virtual void create_timeslice() = 0;
    virtual void clear_last_timeslice() = 0;
    virtual void get_buffer() = 0;
    virtual void get_buffer_size() = 0;
};