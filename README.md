CBM FLES Timeslice Building
===========================

The First-Level Event Selector (FLES) system of the CBM experiment employs a
scheme of timeslices (consisting of microslices) instead of events in data
aquisition. This software aims to implement the timeslice building process
between several nodes of the FLES cluster over an Infiniband network. It is
also used for testbeam readout using the FLES Interface Board (FLIB).


Getting started
---------------

If you are using *flesnet* for the first time or setting up a FLIB-based
readout chain, please read [`INSTALL.md`](INSTALL.md) and
[`HOWTO.md`](HOWTO.md) in this repository. These files will be continuously
updated, so it might be worth checking them from time to time.

CBM scientists: For updates and information concerning *flesnet* and the
operation of a readout chain, please join the
[cbm-flibuser mailing list][flibuser-list] via [this form][list-membership].

[flibuser-list]: https://www-cbm.gsi.de/cbmcdb/display.cgi?obj=grup;view=show;gid=31
[list-membership]: https://www-cbm.gsi.de/cbmcdb/modpers.cgi?view=grup
