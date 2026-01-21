
#include "ItemProducer.hpp"
#include "StorableTimeslice.hpp"
#include "Timeslice.hpp"
#include "TimesliceBuffer.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "TimesliceShmWorkItem.hpp"
#include "TimesliceWorkItem.hpp"
#pragma once

class FragmentedTimesliceBuffer : public TimesliceBuffer {
private:
    std::string shm_identifier_;    ///< shared memory identifier
    boost::uuids::uuid shm_uuid_{}; ///< shared memory UUID
    uint32_t data_buffer_size_exp_; ///< 2's exponent of data buffer size in bytes
    uint32_t desc_buffer_size_exp_; ///< 2's exponent of descriptor buffer size
                                    ///< in units of TimesliceComponentDescriptors
    uint32_t num_input_nodes_;      // number of input nodes
    uint8_t *shm_ptr_ = nullptr;
    std::set<ItemID> outstanding_;
public:

    std::unique_ptr<boost::interprocess::managed_shared_memory> managed_shm_;   ///< shared memory object
    // std::shared_ptr<boost::interprocess::managed_shared_memory> managed_shm_;

    FragmentedTimesliceBuffer(zmq::context_t& context,
                                    const std::string& distributor_address,
                                    std::string shm_identifier,
                                    uint32_t data_buffer_size_exp,
                                    uint32_t desc_buffer_size_exp,
                                    uint32_t num_input_nodes)
                                    : TimesliceBuffer(context, distributor_address, shm_identifier, data_buffer_size_exp, desc_buffer_size_exp, num_input_nodes),
                                    shm_identifier_(std::move(shm_identifier)),
                                    data_buffer_size_exp_(data_buffer_size_exp),
                                    desc_buffer_size_exp_(desc_buffer_size_exp),
                                    num_input_nodes_(num_input_nodes) {
        boost::uuids::random_generator uuid_gen;
        shm_uuid_ = uuid_gen();
        // std::cout << "shm_uuid: " << shm_uuid_ << std::endl;
        boost::interprocess::shared_memory_object::remove(shm_identifier_.c_str());

        std::size_t data_size =
            (UINT64_C(1) << data_buffer_size_exp_) * num_input_nodes_;
        assert(data_size != 0);

        std::size_t desc_buffer_size = (UINT64_C(1) << desc_buffer_size_exp_);
        std::size_t desc_size = desc_buffer_size * num_input_nodes_ *
                          sizeof(fles::TimesliceComponentDescriptor);
        assert(desc_size != 0);

        constexpr size_t overhead_size = 4096; // Wild guess, let's hope it's enough
        size_t managed_shm_size = data_size + desc_size + overhead_size;


        managed_shm_ = std::make_unique<boost::interprocess::managed_shared_memory>(
            boost::interprocess::create_only, shm_identifier_.c_str(),
            managed_shm_size);

        managed_shm_->construct<boost::uuids::uuid>(
            boost::interprocess::unique_instance)(shm_uuid_);

        shm_ptr_ = static_cast<uint8_t*>(managed_shm_->allocate(data_size + desc_size));
    };

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
        outstanding_.insert(ts_pos);
        ItemProducer::send_work_item(ts_pos, ostream.str());
    }

};