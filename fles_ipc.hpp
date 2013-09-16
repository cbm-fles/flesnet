// 2013, Jan de Cuveland <cmail@cuveland.de>

#ifndef FLES_IPC_HPP
#define FLES_IPC_HPP

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <memory>
#include <cstdint>

//! \file
//! This file describes the timeslice-based interface to FLES.

namespace fles {

#pragma pack(1)

//! Microslice descriptor struct.
struct MicrosliceDescriptor
{
    uint8_t   hdr_id;  ///< Header format identifier (0xDD)
    uint8_t   hdr_ver; ///< Header format version (0x01)
    uint16_t  eq_id;   ///< Equipment identifier
    uint16_t  flags;   ///< Status and error flags
    uint8_t   sys_id;  ///< Subsystem identifier
    uint8_t   sys_ver; ///< Subsystem format version
    uint64_t  idx;     ///< Microslice index
    uint32_t  crc;     ///< CRC32 checksum
    uint32_t  size;    ///< Content size (bytes)
    uint64_t  offset;  ///< Offset in event buffer (bytes)
};

//! Timeslice component descriptor struct.
struct TimesliceComponentDescriptor
{
    uint64_t ts_num; ///< Timeslice index.
    uint64_t offset; ///< Start offset (in bytes) of corresponding data.
    uint64_t size;   ///< Size (in bytes) of corresponding data.
};

//! Timeslice work item struct.
struct TimesliceWorkItem
{
    uint64_t ts_pos;                  ///< Start offset (in items) of this timeslice
    uint32_t num_core_microslices;    ///< Number of core microslices
    uint32_t num_overlap_microslices; ///< Number of overlapping microslices
    uint32_t num_components;          ///< Number of components (contributing input channels)
    uint32_t data_buffer_size_exp;    ///< Exp. size (in bytes) of each data buffer
    uint32_t desc_buffer_size_exp;    ///< Exp. size (in bytes) of each descriptor buffer
};

//! Timeslice completion struct.
struct TimesliceCompletion
{
    uint64_t ts_pos; ///< Start offset (in items) of this timeslice
};

#pragma pack()

//! The Timeslice class represents the data of a single timeslice.
class Timeslice
{
    friend class TimesliceReceiver;

    TimesliceWorkItem _work_item;
    TimesliceCompletion _completion{};

    const uint8_t* const _data;
    const TimesliceComponentDescriptor* const _desc;

    std::shared_ptr<boost::interprocess::message_queue> _completions_mq;

    uint64_t _descriptor_offset;
    uint64_t _data_offset_mask;

    const uint8_t& data(uint64_t component, uint64_t offset) const
    {
        return (_data + (component << _work_item.data_buffer_size_exp))[offset & _data_offset_mask];
    }

    const TimesliceComponentDescriptor& desc(uint64_t component) const
    {
        return (_desc + (component << _work_item.desc_buffer_size_exp))[_descriptor_offset];
    }

    Timeslice(TimesliceWorkItem work_item,
              uint8_t* data,
              TimesliceComponentDescriptor* desc,
              std::shared_ptr<boost::interprocess::message_queue> completions_mq) :
        _work_item(work_item),
        _data(data),
        _desc(desc),
        _completions_mq(completions_mq)
    {
        _completion = {_work_item.ts_pos};
        _descriptor_offset = _work_item.ts_pos & ((1L << _work_item.desc_buffer_size_exp) - 1L);
        _data_offset_mask = (1L << _work_item.data_buffer_size_exp) - 1L;
    }

public:

    ~Timeslice()
    {
        _completions_mq->send(&_completion, sizeof(_completion), 0);
    }

    /// Retrieve the timeslice index.
    uint64_t index() const
    { return desc(0).ts_num; }

    /// Retrieve the number of core microslices.
    uint64_t num_core_microslices() const
    { return _work_item.num_core_microslices; }

    /// Retrieve the number of overlapping microslices.
    uint64_t num_overlap_microslices() const
    { return _work_item.num_overlap_microslices; }

    /// Retrieve the total number of microslices.
    uint64_t num_microslices() const
    { return _work_item.num_core_microslices + _work_item.num_overlap_microslices; }

    /// Retrieve the number of components (contributing input channels).
    uint64_t num_components() const
    { return _work_item.num_components; }

    /// Retrieve a pointer to the data content of a given microslice
    const uint8_t* content(uint64_t component, uint64_t microslice) const
    {
        return &data(component, desc(component).offset)
            + num_microslices() * sizeof(MicrosliceDescriptor)
            + descriptor(component, microslice).offset - descriptor(component, 0).offset;
    }

    /// Retrieve the descriptor of a given microslice
    const MicrosliceDescriptor& descriptor(uint64_t component, uint64_t microslice) const
    {
        return (&reinterpret_cast<const MicrosliceDescriptor&>
                (data(component, desc(component).offset)))[microslice];
    }
};

//! The TimesliceReveicer class implements the IPC mechanisms to receive a timeslice.
class TimesliceReceiver
{
    std::unique_ptr<boost::interprocess::shared_memory_object> _data_shm;
    std::unique_ptr<boost::interprocess::shared_memory_object> _desc_shm;

    std::unique_ptr<boost::interprocess::mapped_region> _data_region;
    std::unique_ptr<boost::interprocess::mapped_region> _desc_region;

    std::unique_ptr<boost::interprocess::message_queue> _work_items_mq;
    std::shared_ptr<boost::interprocess::message_queue> _completions_mq;

public:
    /// The TimesliceReceiver default constructor.
    TimesliceReceiver()
    {
        _data_shm = std::unique_ptr<boost::interprocess::shared_memory_object>
            (new boost::interprocess::shared_memory_object
             (boost::interprocess::open_only, "flesnet_data", boost::interprocess::read_only));

        _desc_shm = std::unique_ptr<boost::interprocess::shared_memory_object>
            (new boost::interprocess::shared_memory_object
             (boost::interprocess::open_only, "flesnet_desc", boost::interprocess::read_only));

        _data_region = std::unique_ptr<boost::interprocess::mapped_region>
            (new boost::interprocess::mapped_region(*_data_shm, boost::interprocess::read_only));

        _desc_region = std::unique_ptr<boost::interprocess::mapped_region>
            (new boost::interprocess::mapped_region(*_desc_shm, boost::interprocess::read_only));
        
        _work_items_mq = std::unique_ptr<boost::interprocess::message_queue>
            (new boost::interprocess::message_queue
             (boost::interprocess::open_only, "flesnet_work_items"));

        _completions_mq = std::shared_ptr<boost::interprocess::message_queue>
            (new boost::interprocess::message_queue
             (boost::interprocess::open_only, "flesnet_completions"));
    }

    /// Receive the next timeslice, block if not yet available.
    std::unique_ptr<const Timeslice> receive()
    {       
        TimesliceWorkItem wi;
        std::size_t recvd_size;
        unsigned int priority;

        _work_items_mq->receive(&wi, sizeof(wi), recvd_size, priority);
        assert(recvd_size == sizeof(wi));
        
        return std::unique_ptr<Timeslice>
            (new Timeslice(wi, reinterpret_cast<uint8_t*>(_data_region->get_address()),
                           reinterpret_cast<TimesliceComponentDescriptor*>
                           (_desc_region->get_address()), _completions_mq));
    }
};

} // namespace fles {

#endif // FLES_IPC_HPP
