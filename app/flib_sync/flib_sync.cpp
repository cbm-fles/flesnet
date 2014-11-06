#include "flib.h"

int main()
{
    auto flib = flib_device {0};

    for (auto link : flib.links()) {
        link.prepare_dlm(1); // TODO
    }
    flib.send_dlm_global(); // TODO
}
