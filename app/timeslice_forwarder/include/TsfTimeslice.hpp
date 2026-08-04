#pragma once

#include "OutputArchive.hpp"
#include "StorableTimeslice.hpp"
#include "Timeslice.hpp"

#include <cstdint>

#include <df/BufferMap/BufferMap.hpp>
#include <df/WorkerThread.hpp>
#include <df/Connectors/ConnectorInterface.hpp>

namespace tsforwarder {
class Timeslice : public fles::Timeslice {
public:
    // friend class fles::StorableTimeslice;

    void set_data(std::vector<uint8_t*> data_ptr) {
        fles::Timeslice::data_ptr_ = data_ptr;
    }

    std::vector<uint8_t*> get_data() {
        return data_ptr_;
    }

    std::vector<fles::TimesliceComponentDescriptor*> get_desc() {
        return desc_ptr_;
    }

    fles::TimesliceDescriptor get_timeslice_descriptor() {
        return fles::Timeslice::timeslice_descriptor_;
    }

    void set_desc(std::vector<fles::TimesliceComponentDescriptor*> desc_ptr) {
        fles::Timeslice::desc_ptr_ = desc_ptr;
    }

    void set_timeslice_descriptor(fles::TimesliceDescriptor ts_desc) {
        fles::Timeslice::timeslice_descriptor_ = ts_desc;
    };
};
using TimesliceArchive = fles::OutputArchive<fles::Timeslice, fles::StorableTimeslice, fles::ArchiveType::TimesliceArchive>;
}
