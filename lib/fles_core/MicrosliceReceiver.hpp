// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::MicrosliceReceiver class.
#pragma once

#include "MicrosliceSource.hpp"
#include "StorableMicroslice.hpp"
#include "RingBufferReadInterface.hpp"
#include "RingBuffer.hpp"
#include <string>
#include <memory>

namespace fles
{

/**
 * \brief The MicrosliceReceiver class implements a mechanism to receive
 * Microslices from an InputBufferReadInterface object.
 */
class MicrosliceReceiver : public MicrosliceSource
{
public:
    /// Construct Microslice receiver connected to a given data source.
    MicrosliceReceiver(InputBufferReadInterface& data_source);

    /// Delete copy constructor (non-copyable).
    MicrosliceReceiver(const MicrosliceReceiver&) = delete;
    /// Delete assignment operator (non-copyable).
    void operator=(const MicrosliceReceiver&) = delete;

    virtual ~MicrosliceReceiver(){};

    /**
     * \brief Retrieve the next item.
     *
     * This function blocks if the next item is not yet available.
     *
     * \return pointer to the item, or nullptr if end-of-file
     */
    std::unique_ptr<StorableMicroslice> get()
    {
        return std::unique_ptr<StorableMicroslice>(do_get());
    };

private:
    virtual StorableMicroslice* do_get() override;

    StorableMicroslice* try_get();

    /// Data source (e.g., FLIB).
    InputBufferReadInterface& data_source_;

    uint64_t microslice_index_ = 0;
    uint64_t previous_desc_idx_ = 0;
};
}
