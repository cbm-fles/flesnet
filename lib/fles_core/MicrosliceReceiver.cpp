// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "MicrosliceReceiver.hpp"

namespace fles
{

MicrosliceReceiver::MicrosliceReceiver(DualRingBufferReadInterface& data_source)
    : data_source_(data_source)
{
}

StorableMicroslice* MicrosliceReceiver::try_get()
{
    if (data_source_.desc_buffer().at(microslice_index_ + 1).idx >
        previous_desc_idx_) {

        const volatile MicrosliceDescriptor& desc =
            data_source_.desc_buffer().at(microslice_index_);

        const volatile uint8_t* data_begin =
            &data_source_.data_buffer().at(desc.offset);

        const volatile uint8_t* data_end =
            &data_source_.data_buffer().at(desc.offset + desc.size);

        StorableMicroslice* sms;

        if (data_begin <= data_end) {
            sms = new StorableMicroslice(
                const_cast<const fles::MicrosliceDescriptor&>(desc),
                const_cast<const uint8_t*>(data_begin));
        } else {
            const volatile uint8_t* buffer_begin =
                data_source_.data_buffer().ptr();

            const volatile uint8_t* buffer_end =
                buffer_begin + data_source_.data_buffer().bytes();

            // copy two segments to vector
            std::vector<uint8_t> data;
            data.reserve(desc.size);
            data.assign(data_begin, buffer_end);
            data.insert(data.end(), buffer_begin, data_end);
            assert(data.size() == desc.size);

            sms = new StorableMicroslice(
                const_cast<const fles::MicrosliceDescriptor&>(desc), data);
        }

        ++microslice_index_;

        previous_desc_idx_ =
            data_source_.desc_buffer().at(microslice_index_).idx;

        data_source_.update_ack_pointers(
            data_source_.desc_buffer().at(microslice_index_).offset,
            microslice_index_);

        return sms;
    }
    return nullptr;
}

StorableMicroslice* MicrosliceReceiver::do_get()
{
    if (eof_)
        return nullptr;

    // wait until a microslice is available in the input buffer
    StorableMicroslice* sms = nullptr;
    while (!sms) {
        data_source_.proceed();
        sms = try_get();
    }

    return sms;
}
}
