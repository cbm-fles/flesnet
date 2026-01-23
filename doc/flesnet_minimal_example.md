# Minimal Example Illustrating the Use of Flesnet
## Prerequisites
For these examples, you will need the `mstool`, `flesnet` and `tsclient` binaries.

## Most simple example
What we are going to do in this example:
1. In this example, we will create a microslice archive (`*.msa`) with 10000 microslices in it using the `mstool`.
2. Then, we will use the `mstool` to manage a shared memory region and put the data of the microslice archive into this shared memory region.
3. `flesnet` will then be able to connect to this shared memory region to load the microslices and build timeslices out of them.
5. `flesnet` will put the built timeslices in another shared memory region, from which the `tsclient` will take them and put them into a timeslice archive file (`*.tsa`). Using specific flags ensures that it will automatically stop after building 15 timeslices.

### Creating the microslice archive

The `mstool` is capable of creating a microslice archive file with a specific amount of microslices in it. We will use that archive and the `mstool` to feed our local `flesnet` setup.
To create a microslice archive run:
```
./mstool -n 10000 -p 0 -o ms_archive.msa
```

- `-n`: Amount of microslices to generate.
- `-p`: Use internal pattern generator as source. The argument value is currently unused.
- `-o`: Name of the output file.

### Starting flesnet

First, we will use the `mstool` again to provide microslices using a named shared memory region, and our previously generated microslice archive. Open a terminal and run:
```
./mstool --input-archive ./ms_archive.msa --output-shm fles_in_shared_memory
```
`mstool` will confirm the name of the shared memory in its output:
```
[15:23:55] INFO: providing output in shared memory: fles_in_shared_memory
[15:23:55] INFO: waiting until output shared memory is empty
```
Keep this terminal open.

With our shared memory microslice input source ready, we can start Flesnet. In this most simple example, we will start Flesnet so that one entry node and one build node is represented using one Flesnet process.

```
./flesnet -t zeromq -i 0 -I shm:/fles_in_shared_memory/0 -o 0 -O shm:fles_out_shared_memory/0 --timeslice-size 20 --processor-instances 1 -e "./tsclient -i shm:%s -o file:timeslice_archive.tsa -n 15"
```

- `-t`: We are using the ZeroMQ transport layer to exchange information between entry and build nodes. This makes more sense when we use separate Flesnet instances for entry and build nodes. Here, this single process will do the work of both.
- `-i`: The input index of this `flesnet` instance. Example: When `-i` is set to 2, this entry node will read from the 3rd (indexing starts at 0) input source, defined with the `-I` flag. Again, this makes more sense when having separate entry and build nodes.
- `I`: List of input sources. See use of `-i` flag.
- `-o`: Output index. Similar to the `-i` flag, but refers to the output sink of this Flesnet instance. Example: When `-o` is set to 1, this build node will write the built timeslices into the 2nd (indexing starts at 0) output sink, defined with the `-O` flag. Again, this makes more sense, when having separate entry and build nodes. 
- `-O`: List of output sinks. See use of `-o` flag.
- `--timeslice-size`: Defines how many microslices will make up one timeslice (This does not include overlap. The overlap concept will be explained within a dedicated section).
- `--processor-instances`: The amount of `-e` executions.
- `-e`: Defines the start of the `tsclient` binary. The `tsclient` arguments are:
	- `-i` The source from which the `tsclient` will collect the built timeslices. Here, a template parameter (`%s`) is used. This will be replaced with the respective name of the shared memory, defined in the `-O` list of this `flesnet`  instance; in this case `%s` will be replaced with `flesnet_out_shared_memory`.
	-  `-o`: Output path of the generated timeslice archive file.
	-  `-n`: After this amount of build timeslices, the `tsclient` will stop processing timeslices.

Open a terminal and run the shown command.
When running this command, the output will look similar to the following:
```
./flesnet -t zeromq -i 0 -I shm:/fles_in_shared_memory/0 -o 0 -O shm:fles_out_shared_memory/0 --timeslice-size 20 --processor-instances 1 -e "./tsclient -i shm:%s -o file:timeslice_archive.tsa -n 15" 
[16:04:34] INFO: this is input 0 (of 1)
[16:04:34] INFO: this is output 0 (of 1)
[16:04:34] INFO: timeslice size: 20 microslices
[16:04:34] INFO: number of timeslices: 4294967295
[16:04:34] INFO: using shared memory (fles_in_shared_memory)
[16:04:34] INFO: timeslice buffer 0: fles_out_shared_memory {5c97dff0-3cb1-4480-a2b0-4dbf7526170b}, size: 1 * (128 MiB + 16 MiB) = 144 MiB
[16:04:34] INFO: [i0] |###############_____|__________| 0 B/s (0 Hz)
[16:04:34] INFO: [c0] |____________________|__________| 
[16:04:34] INFO: worker connected: TimesliceAutoSource at PID 44904 (s1/o0/p0/g0)
TimesliceReceiver: opened shared memory fles_out_shared_memory {5c97dff0-3cb1-4480-a2b0-4dbf7526170b}
[16:04:34] INFO: total timeslices processed: 15
[16:04:34] INFO: worker disconnected: TimesliceAutoSource at PID 44904 (s1/o0/p0/g0)
[16:04:34] INFO: exiting
[16:04:35] INFO: [c0] |____________________|__________| 
[16:04:35] INFO: [i0] |....._______________|__________| 99.31 MB/s (9.933 kHz)
[16:04:36] INFO: [c0] |____________________|__________| 
[16:04:36] INFO: [i0] |....._______________|__________| 0 B/s (0 Hz)
[16:04:37] INFO: [c0] |____________________|__________| 
[16:04:37] INFO: [i0] |....._______________|__________| 0 B/s (0 Hz)
[16:04:38] INFO: [c0] |____________________|__________| 
[16:04:38] INFO: [i0] |....._______________|__________| 0 B/s (0 Hz)
[16:04:39] INFO: [c0] |____________________|__________| 
[16:04:39] INFO: [i0] |....._______________|__________| 0 B/s (0 Hz)
[16:04:40] INFO: [c0] |____________________|__________| 
[16:04:40] INFO: [i0] |....._______________|__________| 0 B/s (0 Hz)
^C[16:04:41] INFO: [i0] |....._______________|__________| 0 B/s (0 Hz)
[16:04:41] INFO: exiting
```

Notice the line `[16:04:34] INFO: total timeslices processed: 15`. When the specified amount of timeslices is processed, you can cancel the `flesnet` execution with `Ctrl+C`.

You will now have a `timeslice_archive.tsa` file, which contains 15 timeslices with a size of 20 microslices each.
