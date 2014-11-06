#include <cstdint> // needed before flib.h
#include "flib.h"

static const uint8_t DLM {1};

int main()
{
    flib::flib_device flib {0};

    for (auto link : flib.links()) {
        link->prepare_dlm(DLM, true);
    }
    flib.send_dlm();
}
