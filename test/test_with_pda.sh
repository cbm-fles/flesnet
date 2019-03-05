#!/bin/bash

# test if build includes executables depending on libpda.

if lsmod | grep -q uio_pci_dma; then
    # try to discover FLIBs
    echo "Testing with pda kernel module"
    ./flib_info
else
    # try to start an executable only available if build includes libpda
    # without actually using anything from libpda.
    echo "Testing without pda kernel module"
    ./flib_server -h
fi    
