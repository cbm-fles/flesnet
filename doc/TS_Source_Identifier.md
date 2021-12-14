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


### Parameters of the `shm` scheme

`stride`
: Stride used for selecting which timeslices to receive (default: 1).

`offset`
: Offset used for selecting which timeslices to receive (default: 0).

`queue`
: Specify the queueing mode. Possible values are: `all` (default), `one`, `skip`.

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
