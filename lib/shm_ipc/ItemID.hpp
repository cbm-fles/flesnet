#ifndef SHM_IPC_ITEMID_HPP
#define SHM_IPC_ITEMID_HPP

#include <cstddef>

/**
 * Identifier of a work item, assigned by the producer.
 *
 * This lives in a header of its own so that the producer side does not have to
 * include the full worker protocol definitions.
 */
using ItemID = size_t;

#endif
