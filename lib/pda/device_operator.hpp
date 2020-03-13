/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */
#pragma once

#include <cstdint>
#include <cstdio>

using DeviceOperator = struct DeviceOperator_struct;

namespace pda {

class device_operator {

public:
  device_operator();
  ~device_operator();

  uint64_t device_count();

  DeviceOperator* PDADeviceOperator() { return (m_dop); }

private:
  static const char* m_pci_ids[];
  DeviceOperator* m_dop;
};
} // namespace pda
