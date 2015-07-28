// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "MicrosliceReceiver.hpp"

namespace fles
{

MicrosliceReceiver::MicrosliceReceiver(DataSource& data_source)
    : _data_source(data_source)
{
}

StorableMicroslice* MicrosliceReceiver::try_get()
{
    if (_data_source.desc_buffer().at(_microslice_index + 1).idx >
        _previous_mc_idx) {

        const volatile MicrosliceDescriptor& desc =
            _data_source.desc_buffer().at(_microslice_index);

        const volatile uint8_t* data_begin =
            &_data_source.data_buffer().at(desc.offset);

        const volatile uint8_t* data_end =
            &_data_source.data_buffer().at(desc.offset + desc.size);

        StorableMicroslice* sms;

        if (data_begin < data_end) {
            sms = new StorableMicroslice(
                const_cast<const fles::MicrosliceDescriptor&>(desc),
                const_cast<const uint8_t*>(data_begin));
        } else {
            const volatile uint8_t* buffer_begin =
                _data_source.data_buffer().ptr();

            const volatile uint8_t* buffer_end =
                buffer_begin + _data_source.data_buffer().bytes();

            // copy two segments to vector
            std::vector<uint8_t> data;
            data.reserve(desc.size);
            data.assign(data_begin, buffer_end);
            data.insert(data.end(), buffer_begin, data_end);
            assert(data.size() == desc.size);

            sms = new StorableMicroslice(
                const_cast<const fles::MicrosliceDescriptor&>(desc), data);
        }

        ++_microslice_index;

        _previous_mc_idx = _data_source.desc_buffer().at(_microslice_index).idx;

        _data_source.update_ack_pointers(
            _data_source.desc_buffer().at(_microslice_index).offset,
            _microslice_index);

        return sms;
    }
    return nullptr;
}

StorableMicroslice* MicrosliceReceiver::do_get()
{
    if (_eof)
        return nullptr;

    // wait until a microslice is available in the input buffer
    StorableMicroslice* sms = nullptr;
    while (!sms) {
        _data_source.proceed();
        sms = try_get();
    }

    return sms;
}
}
