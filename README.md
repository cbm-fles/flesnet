CBM FLES Timeslice Building
===========================
![CMake Status](https://github.com/cbm-fles/flesnet/workflows/CMake/badge.svg)
[![Coverage Status](https://coveralls.io/repos/github/cbm-fles/flesnet/badge.svg?branch=master)](https://coveralls.io/github/cbm-fles/flesnet?branch=master)

The First-Level Event Selector (FLES) system of the CBM experiment employs a
scheme of timeslices (consisting of microslices) instead of events in data
aquisition. This software aims to implement the timeslice building process
between several nodes of the FLES cluster over an Infiniband network. It is
also used for testbeam readout using the FLES Interface Board (FLIB).

<div style="border:3px solid #0074d9; background-color:#f0f8ff; padding:12px; margin:12px 0;">
	<strong style="color:#00509e; font-size:1.1em;">Note:</strong>
	<span style="color:#004080;"> This repository now contains the current flesnet code. If you are looking for the legacy flesnet software, please use the tag <code>flesnet-v1.0</code>.</span>
</div>


Getting started
---------------

If you are using *flesnet* for the first time or setting up a FLIB-based
readout chain, please read [`INSTALL.md`](INSTALL.md) and
[`HOWTO.md`](HOWTO.md) in this repository. These files will be continuously
updated, so it might be worth checking them from time to time. Additional
information on how to set up a system for flesnet can be found in the
[flesnet wiki][flesnet-wiki].

CBM scientists: For updates and information concerning *flesnet* and the
operation of a readout chain, please join the
[cbm-flibuser mailing list][flibuser-list] via [this form][list-membership].

[flesnet-wiki]: https://github.com/cbm-fles/flesnet/wiki
[flibuser-list]: https://www-cbm.gsi.de/cbmcdb/display.cgi?obj=grup;view=show;gid=31
[list-membership]: https://www-cbm.gsi.de/cbmcdb/modpers.cgi?view=grup

Development
-----------

Depelopers are encouraged to run the [`contrib/local-setup`] script on their
development machines to properly set the coding style and pre-commit
checks.

