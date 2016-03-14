FLESnet usage for FLIB legacy CBMnet readout
============================================


FLESnet
-------

    cd build
    ./flesnet --help

Modify `flesnet.cfg`:

  - Choose `processor-executable`, e.g., to store data.

#### Usage:
Start `flib_ctrl` to configure FLIB before starting *flesnet*.
Flesnet will automatically read all activated FLIB links.

    ./flesnet

NOTE: In case *flesnet* crashed, exited with an exception, was stopped,
or is just not working, you might need to clean up some things.
See the FAQs for the cleanup procedure.

FLIB Control
------------

    cd build
    ./flib_ctrl --help

Modify `flib_legacy.cfg`:

  - Set the data source for each FLIB link.
    For reading from the optical link use 'link'.
    Note: link 0 is the link closest to the PCIe connector.
  - Set links you do not intend to use to 'disable'.
  - If reading from a optical link you need to provide
    sys\_id and sys\_ver of the connected source.

The provided example `flib_legacy.cfg` will read 8 FLIB links with
patten generator and a microslice size of 100 µs.

#### Usage:
    ./flib_ctrl

Do not start/stop while flesnet is running!

If link is selected as data source, it is mandatory to provide
the subsystem identifier and the subsystem format version
of the connected component. For all other data sources this is
configured automatically. This information can be used to
determine which analysis code is called later on.

For all defined subsystem identifiers, check
`lib/fles_ipc/MicrosliceDescriptor.hpp`.

Control Client
--------------

The control library including `cliclient` is now part of
the CBMroot repository.
Please see the CBMroot repository for further information.


Preparing the FLIB
==================

Download a bit file from the build server
----------------------------------------

run, e.g.:

    curl -O -u <GSI web login user> https://cbm-firmware.gsi.de/result/XX/flib_cnet_readout/XXX/htg_k7_cnet_readout.bit

(NOTE: Failing to provide a valid user/password won't give an error
but result in an unusable file.)
