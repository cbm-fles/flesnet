Installing FLESnet
==================

General remarks: OS and Software Environment
--------------------------------------------

At the moment, the official software environment for operating flesnet with
a FLIB is:

    Debian 8.0 / fairsoft mar15p1

This doesn't imply that it is impossible to run a setup on any other
OS / external packages combination. But tests are only performed for the
official environment. If there are reasons to go to another version please
let me know.

Note: If you would like to configure your front-end using the FLIB, you need
to link against the Controls API library, i.e., `cbmnetcntlserver`, which can
be found in cbmroot. The cbmroot/fairroot version is not fixed.

Earlier versions:

    Debian 7.6 / fairroot external packages jul14p3
    Debian 7.6 / fairroot external packages jul14p2


Getting the Repository
----------------------

    git clone https://github.com/cbm-fles/flesnet.git flesnet


Build Dependencies
------------------

    sudo aptitude install gcc make cmake valgrind \
      doxygen libnuma-dev librdmacm-dev libibverbs-dev git \
      libzmq3-dev libboost1.55-all-dev

### Optional: Fairroot external packages

ZMQ and Boost can alternatively be used from a Fairsoft external packages
installation. See these [installation instructions][fairsoft-ext].
After installing, set `SIMPATH` to point to this installation.

[fairsoft-ext]: http://fairroot.gsi.de/?q=node/8

### InfiniBand

If your machine doesn't feature InfiniBand, you need to install SoftiWARP.
An install script can be found in the flesnet repository:

    cd contrib
    ./install-softiwarp

### Optional: Controls API

If you would like to have controls access to CBMnet via the controls API,
you will need the `cbmnetcntlserver` library found in the CBMroot repository.
(Note: the corresponding make target is `cbmnetcntlserver`.)


Installing the FLIB device driver (PDA)
---------------------------------------

The PDA is the external library which is used to implement the FLIB driver
library. The installation procedure is scripted, just run:

    cd contrib
    ./pda_inst.sh

Note: Users can only access the FLIB if they are member of the group "sudo".
Also, the PDA udev rules might not be active after installing. The sure way to
make it active is to reboot the computer. Other procedures as ...

    udevadm control --reload-rules

... might also work, but this is system dependent. The script always installs
the currently needed version. Please re-run the script if you encounter
compilation errors.


Building flesnet
----------------

For compiling flesnet with Controls API support, set the environment variable
`CNETCNTLSERVER_PATH` to the cbmnetcntlserver library and include location
found in the cbmroot repository, e.g.:

    export CNETCNTLSERVER_PATH=/opt/cbmroot/build/lib:/opt/cbmroot/fles/ctrl

For every new clone of the repository run the following two commands a single
time from the repositories' root directory:

    git submodule init
    git submodule update

Building flesnet:

    mkdir build
    cd build && cmake ..
    make


Preparing the system and OS
---------------------------

IOMMU
-----

Your need to enable your systems IOMMU if not already enabled.
For an enabled IOMMU, following three statements have to be true:

  1. The feature is enabled in your OS.

     For Intel CPUs, you can check `dmesg | grep -e DMAR -e IOMMU`.
     For an enabled IOMMU, you should get something like:

        ACPI: DMAR 0x00000000BE811248 0000B0 (v01 ALASKA A M I    00000001 INTL 00000001)
        Intel-IOMMU: enabled
        dmar: IOMMU 0: reg_base_addr fed91000 ver 1:0 cap c9008020660262 ecap f0105a
        IOAPIC id 0 under DRHD base  0xfed91000 IOMMU 0
        DMAR: No ATSR found
        IOMMU 0 0xfed91000: using Queued invalidation
        IOMMU: Setting RMRR:
        IOMMU: Setting identity map for device 0000:00:1a.0 [0xbf499000 - 0xbf4a7fff]
        IOMMU: Setting identity map for device 0000:00:1d.0 [0xbf499000 - 0xbf4a7fff]
        IOMMU: Prepare 0-16MiB unity mapping for LPC
        IOMMU: Setting identity map for device 0000:00:1f.0 [0x0 - 0xffffff]
        AMD IOMMUv2 driver by Joerg Roedel <joerg.roedel@amd.com>
        AMD IOMMUv2 functionality not available on this system

     For a disabled IOMMU, you should get much less output, e.g.:

        ACPI: DMAR 0x0000000088FFFCC0 0000B4 (v01 A M I  OEMDMAR  00000001 INTL 00000001)
        dmar: IOMMU 0: reg_base_addr fbffc000 ver 1:0 cap d2078c106f0466 ecap f020de
        AMD IOMMUv2 driver by Joerg Roedel <joerg.roedel@amd.com>
        AMD IOMMUv2 functionality not available on this system

     If so, try to set the kernel boot parameter 'intel_iommu=on' and reboot
	 (see below). Note that the output differs form system to system.
     Finding the line `Intel-IOMMU: enabled` is **not** sufficient for an
	 activated IOMMU!

     For AMD CPUs, I could not test this. But you should get something like:

        $ dmesg | grep AMD-Vi
        AMD-Vi: Enabling IOMMU at 0000:00:00.2 cap 0x40
        AMD-Vi: Lazy IO/TLB flushing enabled
        AMD-Vi: Initialized for Passthrough Mode

     The matching kernel parameter should be 'amd_iommu=on'.

     If setting the kernel parameter does not change anything, check 2. and 3.

  2. The CPU supports IOMMU.

     You can check this by running `flesnet/contrib/check-iommu`. This should
	 be true for any modern CPU.

  3. The feature is activated in the BIOS.

     If you run into problems, check the BIOS for features like
     VT-d (Intel), AMD-Vi (AMD), IOMMU or virtualization support and activate
	 them.

Power Management
----------------

On some systems and depending on your BIOS settings, you may encounter
problems with ASPM and ACPI. See also README.md Section *FAQ*: *What to do
if the FLIB/PC becomes unresponsive after programming or a few minutes of use?*

To be on the safe side, please set the kernel boot options

    pcie_aspm=off acpi=ht noapic

Setting boot options for Debian
-------------------------------

Modify `/etc/default/grub`, e.g.:

    GRUB_CMDLINE_LINUX_DEFAULT="pcie_aspm=off acpi=ht noapic intel_iommu=on"
    sudo update-grub
    reboot


Installing Xilinx Software for using a FLIB
===========================================

Xilinx Tools
------------

Install at least the Xilinx Lab Tools (newest version).

    ./xsetup

Don't install cable drivers when asked!

Xilinx Cable drivers
--------------------

    sudo aptitude install libusb-dev fxload
    # go to Xilinx install directory (e.g., /opt/xilinx/14.7/LabTools)
    . settings64.sh
    cd LabTools/bin/lin64/install_script/install_drivers/linux_drivers/pcusb
    sudo ./setup_pcusb 1 (has to say “Using udev…“)
    sudo sed -i -e 's/TEMPNODE/tempnode/' -e 's/SYSFS/ATTRS/g' -e 's/BUS/SUBSYSTEMS/' /etc/udev/rules.d/xusbdfwu.rules
    sudo /etc/init.d/udev restart

If cable drivers are working correctly and a cable is plugged in,
`lsusb` should give you something like:

    ...
    Bus 001 Device 008: ID 03fd:0008 Xilinx, Inc.
    ...

Note the `03fd:0008`.
