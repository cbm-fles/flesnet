#pragma once

#include "MicrosliceDescriptor.hpp"
#include <vector>

namespace fles {

/**
 * Combine microslice metadata and content.
 */
struct MicrosliceContainer {
    MicrosliceContainer(MicrosliceDescriptor d, uint8_t *content);
    /**<
     * Use this constructor to combine an already existing descriptor and
     * allocated memory for the payload. The MicrosliceContainer does not
     * actually contain or own the payload in this case.
     */

    MicrosliceContainer(MicrosliceDescriptor d, std::vector<uint8_t> content);
    /**<
     * Use this constructor to pass ownership of microslice content to
     * a MicrosliceContainer. The descriptor will be updated to match the
     * size of the `content` vector.
     *
     * No vector is copied if `content` is passed as an rvalue,
     * like in
     *     MicrosliceContainer {..., std::move(some_vector)}
     * or
     *     MicrosliceContainer {..., {1, 2, 3, 4, 5}}
     * or
     *     MicrosliceContainer {..., make_vector()}
     */

    MicrosliceDescriptor desc; /**< the microslice descriptor... */
    uint8_t * const content; /**< constant pointer to (non-const) data */

private:
    std::vector<uint8_t> _content;
    /**< stores the data if the second constructor is used */
};

}
