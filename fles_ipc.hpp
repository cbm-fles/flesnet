#pragma once
// 2013, Jan de Cuveland <cmail@cuveland.de>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <memory>
#include <cstdint>

//! \file
//! This file describes the timeslice-based interface to FLES.

namespace fles
{

#pragma pack(1)

//! Microslice descriptor struct.
struct MicrosliceDescriptor
{
    uint8_t hdr_id;  ///< Header format identifier (0xDD)
    uint8_t hdr_ver; ///< Header format version (0x01)
    uint16_t eq_id;  ///< Equipment identifier
    uint16_t flags;  ///< Status and error flags
    uint8_t sys_id;  ///< Subsystem identifier
    uint8_t sys_ver; ///< Subsystem format version
    uint64_t idx;    ///< Microslice index
    uint32_t crc;    ///< CRC32 checksum
    uint32_t size;   ///< Content size (bytes)
    uint64_t offset; ///< Offset in event buffer (bytes)
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
    uint64_t ts_pos; ///< Start offset (in items) of this timeslice
    uint32_t num_core_microslices;    ///< Number of core microslices
    uint32_t num_overlap_microslices; ///< Number of overlapping microslices
    uint32_t num_components; ///< Number of components (contributing input
    /// channels)
    uint32_t data_buffer_size_exp; ///< Exp. size (in bytes) of each data buffer
    uint32_t desc_buffer_size_exp; ///< Exp. size (in bytes) of each descriptor
    /// buffer

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar& ts_pos;
        ar& num_core_microslices;
        ar& num_overlap_microslices;
        ar& num_components;
        ar& data_buffer_size_exp;
        ar& desc_buffer_size_exp;
    }
};

//! Timeslice completion struct.
struct TimesliceCompletion
{
    uint64_t ts_pos; ///< Start offset (in items) of this timeslice
};

#pragma pack()

//! The Timeslice class provides access to the data of a single timeslice.
class Timeslice
{
public:
    Timeslice(const Timeslice&) = delete;
    void operator=(const Timeslice&) = delete;

    ~Timeslice();

    /// Retrieve the timeslice index.
    uint64_t index() const;

    /// Retrieve the number of core microslices.
    uint64_t num_core_microslices() const;

    /// Retrieve the number of overlapping microslices.
    uint64_t num_overlap_microslices() const;

    /// Retrieve the total number of microslices.
    uint64_t num_microslices() const;

    /// Retrieve the number of components (contributing input channels).
    uint64_t num_components() const;

    /// Retrieve a pointer to the data content of a given microslice
    const uint8_t* content(uint64_t component, uint64_t microslice) const;

    /// Retrieve the descriptor of a given microslice
    const MicrosliceDescriptor& descriptor(uint64_t component,
                                           uint64_t microslice) const;

private:
    friend class TimesliceReceiver;
    friend class StorableTimeslice;

    Timeslice(TimesliceWorkItem work_item,
              uint8_t* data,
              TimesliceComponentDescriptor* desc,
              std::shared_ptr
              <boost::interprocess::message_queue> completions_mq);

    const uint8_t& data(uint64_t component, uint64_t offset) const;

    const TimesliceComponentDescriptor& desc(uint64_t component) const;

    TimesliceWorkItem _work_item;
    TimesliceCompletion _completion{};

    const uint8_t* const _data;
    const TimesliceComponentDescriptor* const _desc;

    std::shared_ptr<boost::interprocess::message_queue> _completions_mq;

    uint64_t _descriptor_offset;
    uint64_t _data_offset_mask;
};

//! The StorableTimeslice class contains the data of a single timeslice.
class StorableTimeslice
{
public:
    explicit StorableTimeslice(const Timeslice& ts);

    /// Retrieve the timeslice index.
    uint64_t index() const;

    /// Retrieve the number of core microslices.
    uint64_t num_core_microslices() const;

    /// Retrieve the number of overlapping microslices.
    uint64_t num_overlap_microslices() const;

    /// Retrieve the total number of microslices.
    uint64_t num_microslices() const;

    /// Retrieve the number of components (contributing input channels).
    uint64_t num_components() const;

    /// Retrieve a pointer to the data content of a given microslice
    const uint8_t* content(uint64_t component, uint64_t microslice) const;

    /// Retrieve the descriptor of a given microslice
    const MicrosliceDescriptor& descriptor(uint64_t component,
                                           uint64_t microslice) const;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar& _work_item;
        ar& _data;
        ar& _index;
    }

    TimesliceWorkItem _work_item;
    std::vector<std::vector<uint8_t> > _data;
    uint64_t _index;
};

//! The TimesliceReveicer class implements the IPC mechanisms to receive a
// timeslice.
class TimesliceReceiver
{
public:
    /// The TimesliceReceiver default constructor.
    TimesliceReceiver();

    TimesliceReceiver(const TimesliceReceiver&) = delete;
    void operator=(const TimesliceReceiver&) = delete;

    /// Receive the next timeslice, block if not yet available.
    std::unique_ptr<const Timeslice> receive();

private:
    std::unique_ptr<boost::interprocess::shared_memory_object> _data_shm;
    std::unique_ptr<boost::interprocess::shared_memory_object> _desc_shm;

    std::unique_ptr<boost::interprocess::mapped_region> _data_region;
    std::unique_ptr<boost::interprocess::mapped_region> _desc_region;

    std::unique_ptr<boost::interprocess::message_queue> _work_items_mq;
    std::shared_ptr<boost::interprocess::message_queue> _completions_mq;
};

} // namespace fles {
