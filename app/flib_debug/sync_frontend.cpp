//! \author Michael Krieger

/**
 * This program synchronizes all devices connected to a FLIB by sending
 * the appropriate DLM to all of them simultaneously.
 */

#include "flib.h"

static const uint8_t SYNC_DLM{1};

int main() {
  flib::flib_device_cnet flib{0};

  for (auto link : flib.links()) {
    link->prepare_dlm(SYNC_DLM, true);
  }
  flib.send_dlm();
}
