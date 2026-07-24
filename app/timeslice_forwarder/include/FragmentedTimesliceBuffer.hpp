#pragma once

#include "ItemProducer.hpp"
#include "StorableTimeslice.hpp"
#include "Timeslice.hpp"
#include "TimesliceBuffer.hpp"
#include "TimesliceShmWorkItem.hpp"

#include <boost/archive/binary_oarchive.hpp>
#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>


class FragmentedTimesliceBuffer : public TimesliceBuffer {
public:
    FragmentedTimesliceBuffer(zmq::context_t& context,
                                const std::string& distributor_address,
                                std::string shm_identifier,
                                uint32_t data_buffer_size_exp,
                                uint32_t desc_buffer_size_exp,
                                uint32_t num_input_nodes)
                                : TimesliceBuffer(context, distributor_address, shm_identifier, data_buffer_size_exp, desc_buffer_size_exp, num_input_nodes) {};

    void send_work_item(std::shared_ptr<fles::StorableTimeslice> ts) {
        // Create and fill new TimesliceShmWorkItem to be sent via zmq
        fles::TimesliceShmWorkItem item;
        item.shm_uuid = shm_uuid_;
        item.shm_identifier = shm_identifier_;
        item.ts_desc = ts->timeslice_descriptor_;
        const auto num_components = item.ts_desc.num_components;
        const auto ts_pos = item.ts_desc.ts_pos;
        item.data.resize(num_components);
        item.desc.resize(num_components);
        for (uint32_t c = 0; c < num_components; ++c) {
            item.data[c] = managed_shm_->get_handle_from_address(ts->data_ptr_[c]);
            item.desc[c] = managed_shm_->get_handle_from_address(ts->desc_ptr_[c]);
        }

        std::ostringstream ostream;
        {
            boost::archive::binary_oarchive oarchive(ostream);
            oarchive << item;
        }
        TimesliceBuffer::outstanding_.insert(ts_pos);
        ItemProducer::send_work_item(ts_pos, ostream.str());
    }

    void* get_shm_ptr() {
        return managed_shm_->get_address();
    }

    uint64_t get_shm_size() {
        return managed_shm_->get_size();
    }
};
