# Flesnet Timeslice Source Identifier
(2021-12-13, by Jan de Cuveland)

A timeslice source can be identifed by a UTF8 string that resembles a URI:
```
scheme://path?param1=value1&param2=value2
```

Currently implemented schemes:

`shm`
: Receive timeslice via a local shared memory

`file`
: Read timeslices from .tsa file(s)

`tcp`
: Receive timeslices via tcp network connection

Some Flesnet tools/classes support additional (legacy) adressing schemes.

## The `shm` scheme
Receive timeslice via a local shared memory.
```
Example: shm://identifier
```

Via the shared memory interface, the receivers can access the timeslice data in the computer's main memory without additional duplication or transmission.

The `identifer` selects a shared memory interface on the node. This has to match the `identifier` used by the Flesnet timeslice builder.

The parameters allow for a more detailed specification of the requested timeslices.

Subsampling and round-robin load sharing between receivers are possible using `stride` and `offset`.
We request every timeslice with sequence number _n_ for which exists _m_ in N: _n = m * stride + offset_.

While the receiver retains a handle to a timeslice, the corresponding memory cannot be overwritten. Therefore, independent of the queueing scheme, a slow `shm` client may have to copy the relevant information to local memory buffer and release the handle before processing to not cause backpressure on the readout system.

The sender and the receivers exchange heartbeats continuously, independent of whether timeslices are flowing, and treat a peer that has gone quiet for several heartbeat intervals as dead. A receiver that loses the sender reconnects on its own, so a restarted timeslice builder is picked up without restarting its clients; timeslices produced while no receiver was registered are not retained. When the sender reaches the end of its stream, it tells each receiver so after the last timeslice that receiver will get, which is what lets a client terminate on its own instead of waiting for data that will not come.

The sender and its receivers speak a versioned protocol and must come from the same Flesnet release. A version mismatch is reported and the connection is refused, rather than being misinterpreted.


### Parameters of the `shm` scheme

`stride`
: Stride used for selecting which timeslices to receive (default: 1).

`offset`
: Offset used for selecting which timeslices to receive (default: 0).

`queue`
: Specify the queueing mode. Possible values are: `all` (default), `one`, `skip`.

`group`
: Receivers sharing a non-zero `group` are treated as a group, and each timeslice is sent to only one member of the group (default: 0, meaning no grouping).
This distributes the load over several receivers without any of them seeing a timeslice twice.

`window`
: Number of timeslices the sender may have outstanding for this receiver at the same time (default: 1).
The default reproduces the strict one-at-a-time behaviour, in which the next timeslice is only sent once the previous one has been released.
A larger window lets the receiver work on one timeslice while the next is already on its way, which matters on a connection with a noticeable round-trip time, at the cost of holding more timeslices at once.

**Queue parameter values**

`all`:
: Fully asynchronous, receive all, don't skip. This will create back pressure on the readout system if the receiver is too slow. Most useful for archiving and very fast monitoring.

`one`:
: The newest matching timeslice is kept in an internal 1-item queue while the receiver is not idle.

`skip`:
: There is no queue managed for this receiver. It only receives timeslices that arrive while it is idle.


## The `file` scheme
Read timeslices from one of more .tsa file(s).
```
Example: file:///absolute/path/to/filename.tsa
```

If the filename contains the string `%n`, a sequence of archive files is read. The files are expected to be numbered sequentially starting at zero (0), with the numbers formatted to be at least 4 digits wide (e.g., `file_0000.tsa`).

### Parameters of the `file` scheme

`cycles`
: Repeat reading the input archive in a loop for the given number of times (default: 1).  
This option is meant for performance testing.


## The `tcp` scheme
Receive timeslices via tcp network connection from a specified publisher.
```
Syntax:  tcp://host:port
Example: tcp://localhost:5556
```

Memory for timeslices is allocated dynamically. Depending on the high-water mark setting and the timeslice size, out-of-memory conditions may occur.

The `tcp` scheme has limited performance and is not suitable for the highest data rates.

### Parameters of the `tcp` scheme

`hwm`
: High-water mark for the ZeroMQ subscriber (in timeslices, default: 1)  
Timeslices are dropped if more than this number would have to be buffered.
