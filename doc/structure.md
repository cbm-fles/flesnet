Structure of the project
========================

The project is divided into several libraries:

Libraries with no internal dependencies (leaves)
------------------------------------------------

- crcutil
- cri
- logging
- monitoring
- pda

Libraries with internal dependencies
------------------------------------

- logging
  -> shm_ipc
    -> fles_ipc
      -> fles_tools (with crcutil, monitoring)
      -> tsb_ucx (with UCX)

Roles:

- fles_ipc: the microslice/timeslice API used by consumers (data structures,
  archives, sources and sinks, shared memory reader and writer)
- fles_tools: analysis and debugging tools on top of fles_ipc (pattern
  checkers, analyzers, dumpers)
- tsb_ucx: UCX-specific building blocks of the timeslice building chain
