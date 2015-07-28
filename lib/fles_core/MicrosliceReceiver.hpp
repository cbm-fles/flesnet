// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::MicrosliceReceiver class.
#pragma once

#include "MicrosliceSource.hpp"
#include "StorableMicroslice.hpp"
#include "DataSource.hpp"
#include "RingBuffer.hpp"
#include <string>
#include <memory>

namespace fles
{

/**
 * \brief The MicrosliceReceiver class implements a mechanism to receive
 * Microslices from a DataSource object.
 */
class MicrosliceReceiver : public MicrosliceSource
{
public:
    /// Construct Microslice receiver connected to a given data source.
    MicrosliceReceiver(DataSource& data_source);

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
    DataSource& _data_source;

    uint64_t _microslice_index = 0;
    uint64_t _previous_mc_idx = 0;
};
}
