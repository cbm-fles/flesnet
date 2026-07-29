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

Some basic information about *Flesnet* can be found in the documentation folder [`doc`](./doc).
Start with the documentation landing page at [`doc/README.md`](./doc/README.md), 
then follow the installation guide in [`doc/INSTALL.md`](./doc/INSTALL.md).
For common setup questions, see [`doc/FAQ.md`](./doc/FAQ.md).


Development
-----------

Developers are encouraged to run the [`contrib/local-setup`] script on their
development machines to properly set the coding style and pre-commit
checks.

