Installing FLESnet
==================

General remarks: OS and Software Environment
--------------------------------------------

Flesnet with CRI is designed to run on a Linux operating system.
Although it is possible to use most distributions this guide and
provided scripts focus on Debian/Ubuntu. Especially if planing to use
a CRI it is strongly recommended to use a recent Debian/Ubuntu
installation. This doesn't imply that it is impossible to run a setup
on any other OS but tests are only performed for this environment.

Getting the Repository
----------------------

To get the repository run:

    git clone https://github.com/cbm-fles/flesnet.git

Development follows a rolling release model. The 'master' branch gets
newest developments. If you prefer a slower update cycle (and less
frequent adaptations of your setup) you should use the 'release'
branch:

    git checkout -b release origin/release

Build Dependencies
------------------

The following commands prepare a Ubuntu 18.04 system for flesnet
compilation. For other distributions, please proceed accordingly.

    sudo bash -c "echo 'deb https://download.opensuse.org/repositories/network:/messaging:/zeromq:/release-draft/xUbuntu_18.04/ ./' > /etc/apt/sources.list.d/zmq.list"

    wget https://build.opensuse.org/projects/network:messaging:zeromq:release-draft/public_key -O- | sudo apt-key add

    sudo bash -c "echo -e 'Package: libzmq3-dev\nPin: origin download.opensuse.org\nPin-Priority: 1000\n\nPackage: libzmq5\nPin: origin download.opensuse.org\nPin-Priority: 1000' >> /etc/apt/preferences"

    sudo apt-get update -y

    sudo apt-get install -yq cmake catch doxygen libboost-all-dev \
      libfabric-dev libibverbs-dev libkmod-dev \
      libnuma-dev libpci-dev librdmacm-dev libtool-bin libssl-dev libzmq3-dev

Note: Flesnet currently requires a version of the ZeroMQ library
compiled with "draft" API. The easiest way to install this dependency
is from the zmq build server as shown above.

Note: The minimum required Boost version is 1.65. If you have the
choice between different Boost versions and plan to record data it is
beneficial to use the lowest compatible version. To unpack timeslice
archives the reader's Boost version must not be smaller than the Boost
version used for recording.


### Optional: Include ZeroMQ library in the build flow

If you would like to include downloading and building the ZeroMQ library
in the CMake build flow, you can leave out the ZeroMQ-related commands above.
Instead, when building flesnet, add the following to your cmake statement:

    -DINCLUDE_ZMQ:BOOL=TRUE

Additional build dependencies in this case:

    sudo apt-get install -yq libgnutls28-dev libbsd-dev libsodium-dev


### Optional: Fairroot external packages

ZMQ and Boost can alternatively be used from a Fairsoft external packages
installation. See these [installation instructions][fairsoft-ext].
After installing, set `SIMPATH` to point to this installation.

[fairsoft-ext]: http://fairroot.gsi.de/?q=node/8


### Optional: CRI device driver (PDA)

If you like to use the CRI you need to install the PDA device driver.
The installation procedure is scripted, just run:

    ./contrib/pda_inst.sh

Users can only access the CRI if they are member of the group "pda".
The install script will create a pda system group for you. You should
add all intended users by running something like

    usermod -a -G pda <user>

Note that users need to re-login to refresh their group membership.


Building flesnet
----------------

Building flesnet:

    mkdir build && cd build && cmake ..
    make


Preparing the system and OS
---------------------------

### User limits

You need to increase the 'max locked memory' limit in order to
register enough memory for DMA transfers. To do so for all users you can run

    sudo bash -c 'echo -e "* soft memlock unlimited\n* hard memlock unlimited" > /etc/security/limits.d/10-infiniband.conf'

Note that users need to re-login for new memlock limit to take effect.

### IOMMU

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
     To set it for Debian modify `/etc/default/grub`, e.g.:

            GRUB_CMDLINE_LINUX_DEFAULT="intel_iommu=on"
            sudo update-grub
            reboot

     If setting the kernel parameter does not change anything, check 2. and 3.

  2. The CPU supports IOMMU.

     You can check this by running `flesnet/contrib/check-iommu`. This should
	 be true for any modern CPU.

  3. The feature is activated in the BIOS.

     If you run into problems, check the BIOS for features like
     VT-d (Intel), AMD-Vi (AMD), IOMMU or virtualization support and activate
	 them.

Installing JTAG Software for programming the CRI
-------------------------------------------------

The CRI FPGA can be programmed using any JTAG programmer and matching
software. If using a Xilinx USB II cable with Xilinx software
programming scripts are provided (see [HOWTO.md](HOWTO.md)).

To use these scripts install the Xilinx Vivado Lab Tools or any other
Vivado edition.

Legacy FLIB readout with FLESnet
==================================

Flesnet Version
---------------

The legacy readout isn't supported in newer versions of flesnet
anymore. Versions still supporting the legacy readout are tagged in
the repository. The naming schema for these tags is legacy-ro-vX.X. If
serious issues are fixed a new tag will be created.

You can switch to tags via
    git checkout -b legacy-ro-vX.X legacy-ro-vX.X

