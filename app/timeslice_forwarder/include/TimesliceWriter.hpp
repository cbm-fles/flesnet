#pragma once

#include "OutputArchive.hpp"
#include "StorableTimeslice.hpp"
#include "Timeslice.hpp"
#include "MyTimeslice.hpp"
#include "Tssink.hpp"

#include <cstdint>
#include <memory>

#include <df/BufferMap/BufferMap.hpp>
#include <df/WorkerThread.hpp>
#include <df/Connectors/ConnectorInterface.hpp>
#include "TimesliceShmWorkItem.hpp"
#include "MyTimeslice.hpp"


class TimesliceWriter : public TsSink {
private:
    std::shared_ptr<char> buffer_ = nullptr;
    std::unique_ptr<MyTimesliceArchive> ts_sink_ = nullptr;

public:
    TimesliceWriter(std::string address);

    virtual ~TimesliceWriter();

    uint64_t get_buffer_size() override;

    void write_timeslice(std::vector<BufferMap::ListElement*>& elements) override ;
    void set_buffer(std::shared_ptr<char> buffer);

    std::shared_ptr<char> get_buffer() override;
};
