# Installing Flesnet

## General remarks: OS and Software Environment

Flesnet with CRI is designed to run on a Linux operating system.
Although it is possible to use most distributions, this guide and
provided scripts focus on Debian/Ubuntu. Especially if planning to use
a CRI, it is strongly recommended to use a recent Debian/Ubuntu
installation. This does not mean it is impossible to run a setup
on any other OS, but tests are only performed in this environment.


## Installing from Debian packages

### Prerequisites

To work with the CRI hardware, Flesnet requires the external PDA device driver.
The PDA driver can be found on the [PDA release page][pda-releases].
For Debian-based systems the installation procedure is scripted, just run:

    ./contrib/pda_inst.sh

Users can only access the CRI if they are member of the group "pda".
The installation will create a pda system group for you. You should
add all intended users by running something like

    usermod -a -G pda <user>

Note that users need to re-login to refresh their group membership.

[pda-releases]: https://github.com/cbm-fles/pda/releases


### Installing Flesnet from Debian packages

Flesnet can be installed from Debian packages. Newest packages can be downloaded from the build artifacts of this repository. The main application is contained in the `flesnet-bin` package. To install the packages, run:

    sudo apt install ./flesnet-bin_*.deb


## Building from source

You can optionally build Flesnet from source.
To get an idea of the build process, you can check the [CI build flow][ci-build].

[ci-build]: .github/workflows/cmake.yml


### Optional: Fairroot external packages

ZMQ and Boost can alternatively be used from a Fairsoft external packages
installation. See these [installation instructions][fairsoft-ext].
After installing, set `SIMPATH` to point to this installation.

[fairsoft-ext]: http://fairroot.gsi.de/?q=node/8


### Optional: Building the Timeslice Forwarder

Before building the Timeslice Forwarder make sure the following libraries are available on your system:

- libibverbs
- libibverbs-devel
- librdmacm
- librdmacm-devel

To build the Timeslice Forwarder use the `BUILD_TIMESLICE_FORWARDER=ON` CMake option, e.g.:

```
mkdir build
cd build
cmake .. -DBUILD_TIMESLICE_FORWARDER=ON
```

Use the `--help` option of the `timeslice_forwarder` to see the usage instructions.

### Optional: Building the Archive Validator

To build the Timeslice Forwarder use the following `BUILD_ARCHIVE_VALIDATOR` CMake option, e.g.:

```
mkdir build
cd build
cmake .. -DBUILD_ARCHIVE_VALIDATOR=ON
```

Use the `--help` option of the `archive_validator` to see the usage instructions.

## Preparing the system and OS

In order to use Flesnet with CRI, the system needs to be prepared.


### User limits

You need to increase the 'max locked memory' limit in order to
register enough memory for DMA transfers. To do so for all users, you can run

    sudo bash -c 'echo -e "* soft memlock unlimited\n* hard memlock unlimited" > /etc/security/limits.d/10-infiniband.conf'

Note that users need to re-login for new memlock limit to take effect.


### IOMMU

You need to enable your system's IOMMU if it is not already enabled.
For IOMMU to be enabled, the following three statements must be true:

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
	 (see below). Note that the output differs from system to system.
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

     If setting the kernel parameter does not change anything, check items 2 and 3.

  2. The CPU supports IOMMU.

     You can check this by running `flesnet/contrib/check-iommu`. This should
	 be true for any modern CPU.

  3. The feature is activated in the BIOS.

     If you run into problems, check the BIOS for features like
     VT-d (Intel), AMD-Vi (AMD), IOMMU or virtualization support and activate
	 them.

