//! \author Michael Krieger
//! Representation of the "packed DTM" format used in FLIB-DPB microslices.

/**
 * Data format as defined in flib/hw/src/cbmnet/cbmnet_packer.vhd:
 * (see also: D. Hutter, CBM Collaboration Meeting, 04/2014)
 *
 *     nnll dddd dddd dddd
 *     dddd dddd dddd dddd
 *     ...
 *     dddd pppp pppp pppp
 *
 * where
 * - `nn`:   8-bit DTM index
 * - `ll`:   8-bit DTM length (number of `dddd` words - 1)
 * - `dddd`: DTM payload (16-bit words)
 * - `pppp`: padding to multiple of 64 bits
 *
 * Example:
 *
 *     nn04 dddd dddd dddd
 *     dddd dddd pppp pppp
 *
 * In practice, the first word of the payload (from the microslice point
 * of view) is the CBMnet source address `aaaa`, and the remaining words are the
 * actual payload of the DTM.
 *
 * With this in mind, the `ll` field can more naturally be interpreted as
 * the number of `dddd` words (excluding `aaaa`):
 *
 *     nnll aaaa dddd dddd
 *     dddd dddd dddd dddd
 *     ...
 *     dddd pppp pppp pppp
 */

#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace flib_dpb {

/**
 * Representation of DTM contents.
 * Referring to the above description, `data` points to the `aaaa` word
 * and `size` is the number of words forming the payload *including*
 * `aaaa`.
 */
struct DTM {
    const uint16_t *data;
    size_t size;
};

struct MicrosliceContents {
    // TODO construct from MicrosliceDescriptor (but it lacks the data)
    MicrosliceContents(const uint8_t *ms_data, size_t ms_size);
    const std::vector<DTM>& dtms() const { return _dtms; };
private:
    std::vector<DTM> _dtms;
};

} // namespace
