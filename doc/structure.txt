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
      -> fles_core (with crcutil, monitoring)
        -> fles_libfabric
        -> fles_rdma
        -> fles_zeromq
        -> flib_ipc
