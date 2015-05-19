// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ArchiveDescriptor.hpp"
#include "Microslice.hpp"
#include "MicrosliceDescriptor.hpp"
#include <vector>
#include <fstream>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
// Note: <fstream> has to precede boost/serialization includes for non-obvious
// reasons to avoid segfault similar to
// http://lists.debian.org/debian-hppa/2009/11/msg00069.html

namespace fles
{

template <class Base, class Derived, ArchiveType archive_type>
class InputArchive;

/**
 * Store microslice metadata and content in a single object.
 */
class StorableMicroslice : public Microslice
{
public:
    StorableMicroslice(const StorableMicroslice& ms);
    void operator=(const StorableMicroslice&) = delete;
    StorableMicroslice(StorableMicroslice&& ms);

    StorableMicroslice(const Microslice& ms);

    StorableMicroslice(MicrosliceDescriptor d, const uint8_t* content);
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

private:
    friend class boost::serialization::access;
    friend class InputArchive<Microslice, StorableMicroslice,
                              ArchiveType::MicrosliceArchive>;

    StorableMicroslice();

    template <class Archive>
    void serialize(Archive& ar, const unsigned int /* version */)
    {
        ar& _desc;
        ar& _content;

        init_pointers();
    }

    void init_pointers()
    {
        _desc_ptr = &_desc;
        _content_ptr = _content.data();
    }

    MicrosliceDescriptor _desc;
    std::vector<uint8_t> _content;
};

} // namespace fles {
