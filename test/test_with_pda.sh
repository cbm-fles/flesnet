#!/bin/bash

# test if build includes executables depending on libpda.

if lsmod | grep -q uio_pci_dma; then
    # try to discover CRIs
    echo "Testing with pda kernel module"
    ./cri_info
else
    # try to start an executable only available if build includes libpda
    # without actually using anything from libpda.
    echo "Testing without pda kernel module"
    ./cri_server -h
fi    
