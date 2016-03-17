// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::MicrosliceReceiver class.
#pragma once

#include "DualRingBuffer.hpp"
#include "MicrosliceSource.hpp"
#include "RingBuffer.hpp"
#include "StorableMicroslice.hpp"
#include <memory>
#include <string>

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
    explicit MicrosliceReceiver(InputBufferReadInterface& data_source);

    /// Delete copy constructor (non-copyable).
    MicrosliceReceiver(const MicrosliceReceiver&) = delete;
    /// Delete assignment operator (non-copyable).
    void operator=(const MicrosliceReceiver&) = delete;

    ~MicrosliceReceiver() override = default;

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
    StorableMicroslice* do_get() override;

    StorableMicroslice* try_get();

    /// Data source (e.g., FLIB).
    InputBufferReadInterface& data_source_;

    uint64_t write_index_desc_ = 0;
    uint64_t read_index_desc_ = 0;
};
} // namespace fles
