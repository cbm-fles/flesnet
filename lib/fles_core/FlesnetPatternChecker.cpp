// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>

#include "FlesnetPatternChecker.hpp"

bool FlesnetPatternChecker::check(const fles::Microslice& m)
{
    const uint64_t* content = reinterpret_cast<const uint64_t*>(m.content());
    uint32_t crc = 0x00000000;
    for (size_t pos = 0; pos < m.desc().size / sizeof(uint64_t); ++pos) {
        uint64_t data_word = content[pos];
        crc ^= (data_word & 0xffffffff) ^ (data_word >> 32);
        uint64_t expected =
            (static_cast<uint64_t>(component) << 48) | (pos * sizeof(uint64_t));
        if (data_word != expected) {
            return false;
        }
    }
    return crc == m.desc().crc;
}
