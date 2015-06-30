/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */

#pragma once

#include <flib_device.hpp>

namespace flib {

class flib_device_flesin : public flib_device {

  constexpr std::array<uint16_t, 1> hw_ver_table = {{20}};

};

} /** namespace flib */
