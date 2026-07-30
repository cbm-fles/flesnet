# Frequently Asked Questions


### What to do if the CRI/PC gets unresponsive after programming or a few
  minutes of use?

  - Set the Linux boot parameters: 'pcie_aspm=off 'acpi=ht noapic'.
  - Try to disable ASPM in the BIOS settings of your host PC.

### My syslog/kernel log is flooded with errors like:

    flip00 kernel: [  573.734102] pcieport 0000:00:01.0: AER: Corrected error received: id=0008
    flip00 kernel: [  573.734113] pcieport 0000:00:01.0: PCIe Bus Error: severity=Corrected, type=Physical Layer, id=0008(Receiver ID)
    flip00 kernel: [  573.734115] pcieport 0000:00:01.0:   device [8086:0e02] error status/mask=00000001/00002000
    flip00 kernel: [  573.734117] pcieport 0000:00:01.0:    [ 0] Receiver Error         (First)

  - Set the Linux boot parameters: 'pcie_aspm=off 'acpi=ht noapic'
