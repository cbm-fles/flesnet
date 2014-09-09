#pragma once

#include "MicrosliceDescriptor.hpp"
#include <vector>

namespace fles {

struct MicrosliceView;

/**
 * Store microslice metadata and content in a single object.
 */
struct StorableMicroslice {
    StorableMicroslice(const MicrosliceView& mc);

    StorableMicroslice(MicrosliceDescriptor d, const uint8_t *content);
    /**<
     * Copy the descriptor and the data pointed to by `content` into the
     * StorableMicroslice. The `size` field of the descriptor must already
     * be valid and will not be modified.
     */

    StorableMicroslice(MicrosliceDescriptor d, std::vector<uint8_t> content);
    /**<
     * Copy the descriptor and copy or move the data contained in
     * `content` into the StorableMicroslice. The descriptor will be
     * updated to match the size of the `content` vector.
     *
     * Copying the vector is avoided if it is passed as an rvalue,
     * like in
     *     StorableMicroslice {..., std::move(some_vector)}
     * or
     *     StorableMicroslice {..., {1, 2, 3, 4, 5}}
     * or
     *     StorableMicroslice {..., create_some_vector()}
     */

    MicrosliceDescriptor& desc() { return _desc; };
    //< must be writable, e.g. by StorableTimeslice::append_microslice
    const uint8_t *content() const { return _content.data(); };

private:
    MicrosliceDescriptor _desc;
    std::vector<uint8_t> _content;
};

}
