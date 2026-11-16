# Simulated and recorded microslice streams

The CBM simulation produces synthetic detector raw data as microslice
streams, one `.msa` file (or file sequence) per FLIM link. Flesnet can turn
these streams into timeslices in two ways:

- **Offline, in a single process:** the `msa` source scheme builds timeslices
  deterministically from the files, in `tsclient`, `cbmreco`, or any program
  using `fles::TimesliceAutoSource`. See
  [`TS_Source_Identifier.md`](./TS_Source_Identifier.md#the-msa-scheme).
- **Online, through the real chain:** `stserver` replays the streams as input
  channels, and `tsmanager`/`tsbuilder` build the timeslices over the network
  exactly as in production. This serves the vertical test, stability tests
  and throughput tests on realistic data.

Both cut the timeslices with the same rules (`lib/fles_ipc/ComponentWindow.hpp`).

## Time in the chain

Timeslice building is keyed on time: the start time of every microslice
(`MicrosliceDescriptor::idx`, nanoseconds) decides which timeslice it belongs
to, and the online chain derives the timeslice to build next from the current
time. Timeslices of a stream whose timestamps are far from "now" are either
lost as too old or never complete.

The chain therefore reads a configurable **data clock** instead of the system
clock:

    data_time = data_time_origin + (system_time - data_time_reference) * rate

The three processes `stserver`, `tsmanager` and `tsbuilder` take the same
options (also in their config files) and must be given identical values:

| option                  | meaning                                                                  | default         |
|-------------------------|--------------------------------------------------------------------------|-----------------|
| `--data-time-origin`    | data time read at the reference instant (ns or ISO 8601)                 | the reference   |
| `--data-time-reference` | system time at which the data clock reads the origin (ns or ISO 8601)    | the Unix epoch  |
| `--data-clock-rate`     | speed of the data clock relative to the system clock                     | 1               |

With the defaults the data clock is the system clock. The data time never goes
below zero; until the data clock reaches zero the chain idles. Monitoring and
logging always use the system clock. A rate below 1 slows the whole chain
down, e.g. to replay a stream from disk that could not be read at full rate.

Example: a stream whose timestamps start at zero, to start 30 s after launch
at a quarter of the real rate:

    REF=$(( $(date +%s%N) + 30000000000 ))
    CLOCK="--data-time-origin 0 --data-time-reference $REF --data-clock-rate 0.25"
    tsmanager $CLOCK ...
    tsbuilder $CLOCK ...
    stserver  $CLOCK --replay-input '/data/sim/link_*.msa' ...

## Replaying streams in stserver

    --replay-input URI     a .msa file, a %n file sequence, or a glob pattern;
                           one input channel per matching file; repeatable
    --replay-shift         shift the timestamps so that the streams start at
                           the current data time
    --replay-loop SIZE     load up to SIZE bytes of content per channel and
                           replay them in a loop (implies --replay-shift)

Each channel writes its microslices into the readout buffers when the data
clock reaches their start time, so the stream arrives at its original rate.
Like the pattern generator, a replay channel never blocks: microslices that do
not fit into the buffers are dropped and counted (`skipped_microslices` in the
status metrics), and the next microslice carries the `OverflowFlim` flag.

Replay channels can be combined with pattern generator channels. When all
channels are replay channels and every stream has been played once, the
stserver finishes by itself, so a replay run terminates on its own.

Three modes:

1. **Faithful** (default): timestamps are kept, the data clock is configured
   to match the streams (see above). Detector content and descriptors stay
   consistent, so the built timeslices can be reconstructed. This is the mode
   for the vertical test and for stability tests.
2. **Shifted** (`--replay-shift`): the timestamps are moved by a whole number
   of timeslices so that every stream starts in the first timeslice at least
   one second of data time after the stserver started; every microslice keeps
   its position within its timeslice. No data clock setup is needed.
3. **Looped** (`--replay-loop`): the first SIZE bytes of content of every
   channel are loaded into memory and replayed again and again, each cycle
   shifted by the whole timeslices the segment spans. This removes the disk
   from throughput tests; load whole timeslices to loop seamlessly. The data
   rate is the rate of the simulated data; use `--data-clock-rate` above 1 to
   speed it up, or several channels on the same file.

The microslice descriptors can be shifted freely, the detector content cannot:
the unpackers of STS and MUCH derive the epoch cycle from the microslice start
time (safe only for shifts that are multiples of ~28.6 min), those of TOF and
BMON reject microslices whose first epoch message does not match the start
time (multiples of ~7.2 min), and MVD frames carry absolute timestamps. TRD,
TRD2D, RICH and MUST only use times relative to the microslice start. Shifted
and looped data is therefore for timeslice building tests, not for
reconstruction.

Note for the simulation: TOF and BMON expect the timeslice start to be a
multiple of their epoch (25.6 µs), which the default timeslice duration of
40 ms is not. Use a compatible duration such as 12.8 ms or 102.4 ms for
simulated runs that go through these unpackers.

## Generating test streams

`mstool` writes pattern generator streams with time-like start times:

    mstool -p 0 -n 3000 --microslice-duration 1ms --start-time 0 -o link0.msa
    mstool -p 1 -n 3000 --microslice-duration 1ms --start-time 0 -o link1.msa

and `tsclient` builds timeslices from them:

    tsclient -i 'msa://./link*.msa?duration=10ms' -a -o file://out.tsa
