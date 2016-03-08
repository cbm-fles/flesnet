FLESnet usage for FLIB stand alone readout
==========================================

Note the **FAQ** section at the end of the document!

For information regarding the legacy CBMnet readout please also refer
to [HOWTO_LEGACY.md](HOWTO_LEGACY.md).


FLESnet
-------

    cd build
    ./flesnet --help

Modify `flesnet.cfg`:

  - Choose `processor-executable`, e.g., to store data.

#### Usage:
Start `flib_cfg` to configure FLIB before starting *flesnet*.
Flesnet will automatically read all activated FLIB links.

    ./shm_flesnet

NOTE: In case *flesnet* crashed, exited with an exception, was stopped,
or is just not working, you might need to clean up some things.
See the FAQs for the cleanup procedure.

The flesnet status output shows the fill level of the used
buffers. The interpretation of the symbols is:

    # used    (written - sent)
    x sending (sent - acked)
    . freeing (acked - cached_acked)
    _ unsused (cached_acked + size - written)

FLIB Config
------------

    cd build
    ./flib_cfg --help

Modify `flib.cfg`:

  - Set the data source for each FLIB link.
    For reading from a FLIM use 'flim'.
    Note: link 0 is the link closest to the PCIe connector.
  - Set links you do not intend to use to 'disable'.

#### Usage:
    ./flib_cfg

Do not start/stop while flesnet is running!

Enable readout
--------------

After the FLIB is configured and shm_flesnet is started you need to
enable the readout via

    ./en_readout


Preparing the FLIB
==================

Download a bit file from the build server
----------------------------------------

run, e.g.:

    curl -O -u <GSI web login user> https://cbm-firmware.gsi.de/result/XXXX/flib_flesin/XXX/htg_k7_flesin.bit

(NOTE: Failing to provide a valid user/password won't give an error
but result in an unusable file.)

Programming the FPGA
--------------------

Copy `contrib/flib-jtag` to a location in your path and adopt settings.

You can:

  - Add the command to call impact
  - Fix the Xilinx Cable serial number
  - Choose if you like to use a CSE server
  - Choose if you like to run the script remotely

Note: Do not source Xilinx `settings.sh` in the environment/shell you
like to run/build flesnet! This will replace your glibc and cause
problems. You might add the full path to impact to flib-jtag instead.

#### Usage:
    flib-jtag prog path/to/bitfile.bit

If programming the FPGA the first time since powerup or
your machine does not support PCIe hot-plugging, you need to reboot
the machine after programming.

Check if FLIB is operational
----------------------------

The script `contrib/flib-check` should give you something like:

```
02:00.0 Signal processing controller [1108]: CERN/ECP/EDU Device beaf
	Subsystem: CERN/ECP/EDU Device beaf
	Control: I/O- Mem+ BusMaster+ SpecCycle- MemWINV- VGASnoop- ParErr- Stepping- SERR- FastB2B- DisINTx-
	Status: Cap+ 66MHz- UDF- FastB2B- ParErr- DEVSEL=fast >TAbort- <TAbort- <MAbort- >SERR- <PERR- INTx-
	Latency: 0
	Interrupt: pin A routed to IRQ 32
	Region 0: Memory at ca000000 (32-bit, non-prefetchable) [size=32M]
	Region 1: Memory at cc000000 (32-bit, non-prefetchable) [size=4M]
	Capabilities: [40] Power Management version 3
		Flags: PMEClk- DSI- D1- D2- AuxCurrent=0mA PME(D0+,D1+,D2+,D3hot+,D3cold-)
		Status: D0 NoSoftRst+ PME-Enable- DSel=0 DScale=0 PME-
	Capabilities: [48] MSI: Enable- Count=1/4 Maskable- 64bit+
		Address: 0000000000000000  Data: 0000
	Capabilities: [60] Express (v2) Endpoint, MSI 00
		DevCap:	MaxPayload 256 bytes, PhantFunc 0, Latency L0s <64ns, L1 unlimited
			ExtTag- AttnBtn- AttnInd- PwrInd- RBE+ FLReset-
		DevCtl:	Report errors: Correctable- Non-Fatal- Fatal- Unsupported-
			RlxdOrd+ ExtTag- PhantFunc- AuxPwr- NoSnoop+
			MaxPayload 128 bytes, MaxReadReq 512 bytes
		DevSta:	CorrErr- UncorrErr- FatalErr- UnsuppReq- AuxPwr- TransPend-
		LnkCap:	Port #0, Speed 5GT/s, Width x8, ASPM L0s, Latency L0 unlimited, L1 unlimited
			ClockPM- Surprise- LLActRep- BwNot-
		LnkCtl:	ASPM Disabled; RCB 64 bytes Disabled- Retrain- CommClk+
			ExtSynch- ClockPM- AutWidDis- BWInt- AutBWInt-
		LnkSta:	Speed 5GT/s, Width x8, TrErr- Train- SlotClk+ DLActive- BWMgmt- ABWMgmt-
		DevCap2: Completion Timeout: Range B, TimeoutDis-
		DevCtl2: Completion Timeout: 50us to 50ms, TimeoutDis-
		LnkCtl2: Target Link Speed: 5GT/s, EnterCompliance- SpeedDis-, Selectable De-emphasis: -6dB
			 Transmit Margin: Normal Operating Range, EnterModifiedCompliance- ComplianceSOS-
			 Compliance De-emphasis: -6dB
		LnkSta2: Current De-emphasis Level: -6dB, EqualizationComplete-, EqualizationPhase1-
			 EqualizationPhase2-, EqualizationPhase3-, LinkEqualizationRequest-
	Capabilities: [100 v1] Device Serial Number 00-00-00-00-00-00-00-00
	Kernel driver in use: uio_pci_dma
```

Check if:

  - 'LnkSta:' shows 'Speed 5GT/s' and 'Width x8'. Otherwise the card is not
    running with full capabilities. Try to replug the card or switch to another
	PCIe slot.
  - After you used the FLIB/flesnet the first time, 'Kernel driver in use'
    shows 'uio\_pci\_dma'.


FAQ
===

### What should I do if Flesnet exits with:

    ERROR: rdma_bind_addr(port=20079) failed: Cannot assign requested address
	terminate called without an active exception

  - Check if there are leftover processes for a previous run.
  - Check if *localhost* is set properly in `/etc/hosts` or
    try to use *127.0.0.1* instead of *localhost* in  `flesnet.cfg`.


### What should I do if something went wrong?

If *flesnet* crashed, exited with an exception, was stopped,
or is just not working, you might need to clean up some things.
Try to one or more of the following:

  - Check if there are left over processes and kill them.
    Most likely there is still a *tsclient* process.
  - Remove leftover shared memories if present:
    `sudo rm /dev/shm/flesnet_*`
  - Re-program the FLIB FPGA.


### How can I check if the FLIB is installed correctly?

See the *Check* Section in *Preparing the FLIB*.


### What to do if the FLIB/PC gets unresponsive after programming or a few
  minutes of use?

  - Set the Linux boot parameters: 'pcie_aspm=off 'acpi=ht noapic'.
  - Try to disable ASPM in the BIOS settings of your host PC.

### My syslog/kernel log is flooded with errors like:

    flip00 kernel: [  573.734102] pcieport 0000:00:01.0: AER: Corrected error received: id=0008
    flip00 kernel: [  573.734113] pcieport 0000:00:01.0: PCIe Bus Error: severity=Corrected, type=Physical Layer, id=0008(Receiver ID)
    flip00 kernel: [  573.734115] pcieport 0000:00:01.0:   device [8086:0e02] error status/mask=00000001/00002000
    flip00 kernel: [  573.734117] pcieport 0000:00:01.0:    [ 0] Receiver Error         (First)

  - Set the Linux boot parameters: 'pcie_aspm=off 'acpi=ht noapic'


### Flesnet can not allocate enough memory.

  - The softiwarp install script installs a configuration to /etc/security/limits.d/.
    You need to re-login after installing for these changes to take affect.
  - If the problem is still present, try `sudo ulimit -l unlimited` as a workaround.


### Programming the FLIB FPGA is very slow (takes longer than 60s).

- There is an issue with the Xilinx Lab Tools.
  Try this [solution from the Xilinx forums][impact-fix].

[impact-fix]: http://forums.xilinx.com/t5/Design-Tools-Others/iMPACT-severe-TCK-frequency-derating-under-Linux-still-the-same/m-p/422263#M5692

### The FLIB programming script reports the warning:

    Warning: Chain frequency (1000000) is less than the current cable speed (6000000).
     Adjust to cable speed (1000000).
    Maximum TCK operating frequency for this device chain: 1000000.

  - See 'Programming the FLIB FPGA is very slow'.


### The PC restarts immediately after I try to shut it down.

This is a hardware issue with HTG-K7 rev 1.0 boards. It is caused by the PCIe
wake signal changing its state when the FPGA looses configuration after power
down.

Known workarounds:

  - Unplug the power after shutting down.
  - Try to disable PCIe wake capability, e.g. by changing wake-on-lan BIOS
    settings.
  - Isolate the wake# pin on the PCIe connector.
