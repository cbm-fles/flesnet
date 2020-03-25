// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "StorableMicroslice.hpp"
#include "MicrosliceView.hpp"

namespace fles {

StorableMicroslice::StorableMicroslice(const StorableMicroslice& ms)
    : Microslice(), desc_(ms.desc_), content_(ms.content_) {
  init_pointers();
}

StorableMicroslice::StorableMicroslice(StorableMicroslice&& ms) noexcept
    : desc_(ms.desc_), content_(std::move(ms.content_)) {
  init_pointers();
}

StorableMicroslice::StorableMicroslice(const Microslice& ms)
    : StorableMicroslice{ms.desc(), ms.content()} {}

StorableMicroslice::StorableMicroslice(MicrosliceDescriptor d,
                                       const uint8_t* content_p)
    : desc_(d), // cannot use {}, see http://stackoverflow.com/q/19347004
      content_{content_p, content_p + d.size} {
  init_pointers();
}

StorableMicroslice::StorableMicroslice(MicrosliceDescriptor d,
                                       std::vector<uint8_t> content_v)
    : desc_(d), content_{std::move(content_v)} {
  desc_.size = static_cast<uint32_t>(content_.size());
  init_pointers();
}

StorableMicroslice::StorableMicroslice() = default;

void StorableMicroslice::initialize_crc() { desc_.crc = compute_crc(); }

} // namespace fles
