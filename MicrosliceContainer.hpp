#include "MicrosliceDescriptor.hpp"
#include <vector>

namespace fles {

/**
 * Combines microslice metadata plus payload.
 */
struct MicrosliceContainer {
    MicrosliceDescriptor desc;
    std::vector<uint8_t> content;

    MicrosliceContainer(MicrosliceDescriptor d, std::vector<uint8_t> c)
    : desc (d), // cannot use {}, see http://stackoverflow.com/q/19347004
      content {std::move(c)}
    {
        desc.size = content.size();
    };
    /**<
     * no vector is copied if an rvalue is passed for c,
     * like in
     *     MicrosliceContainer {..., std::move(some_vector)}
     * or
     *     MicrosliceContainer {..., {1, 2, 3, 4, 5}}
     * or
     *     MicrosliceContainer {..., make_vector()}
     */
};

}
