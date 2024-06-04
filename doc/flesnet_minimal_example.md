# Minimal Example Illustrating the Use of FLESnet
## Prerequisites
For these examples, you will need the `mstool`, `flesnet` and `tsclient` binaries.

## Most simple example
What we are going to do in this example:
1. In this example, we will create a microslice archive (`*.msa`) with 10000 microslices in it using the `mstool`.
2. Then, we will use the `mstool` to manage a shared memory region and put the data of the microslice archive into this shared memory region.
3. `flesnet` will then be able to connect to this shared memory region to load the microslices and build timeslices out of them.
5. `flesnet` will put the built timeslices in another shared memory region, from which the `tsclient` will take them and put them into a timeslice archive file (`*.tsa`). Using specific flags ensures that it will automatically stop after building 15 timeslices.

## Creating the Microslice Archive

The `mstool` is capable of creating a microslice archive file with a specific amount of microslices in it. We will use that archive and the `mstool` to feed our local `flesnet` setup.
To create a microslice archive run:
```
./mstool -n 10000 -p 0 -o ms_archive.msa
```

- `-n`: Amount of microslices to generate.
- `-p`: Use internal pattern generator as source. The argument value is currently unused.
- `-o`: Name of the output file.

## Running Flesnet

Flesnet is a distributed system which mainly constists of entry and build nodes. The following chapters will explain how to start these types of nodes.
We will run the minimal configuration of Flesnet by running one Entry Node and one Build Node.
Entry nodes are responsible for taking microslices from a shared memory region and sending them to the build nodes.
Build nodes will build timeslices out of the received microslices.

### Start the build node

```
./flesnet -t zeromq --timeslice-size 20 -n 15 -o 0 -I shm://127.0.0.1/0 -O shm:/fles_out_shared_memory/0 --processor-instances 1 -e "./tsclient -i shm:%s -o file:timeslice_archive.tsa"
```

- `-t`: We are using the ZeroMQ transport layer to exchange information between entry and build nodes. This makes more sense when we use seperate FLESnet instances for entry and build nodes. Here, this singke process will do the work of both.
- `-o`: Output index. Refers to the output sink of this Flesnet instance. Example: When `-o` is set to 1, this build node will write the built timeslices into the 2nd (indexing starts at 0) output sink, defined with the `-O` flag.
- `-O`: List of output sinks. See use of `-o` flag.
- `--timeslice-size`: Defines how many microslices will make up one timeslice (This does not include overlap. The overlap concept will be explained within a dedicated section).
- `--processor-instances`: The amount of `-e` executions.
- `-e`: Defines the start of the `tsclient` binary. The `tsclient` arguments are:
	- `-i` The source from which the `tsclient` will collect the built timeslices. Here, a template parameter (`%s`) is used. This will be replaced with the respective name of the shared memory, defined in the `-O` list of this `flesnet`  instance; in this case `%s` will be replaced with `fles_out_shared_memory`.
	-  `-o`: Output path of the generated timeslice archive file.

The fact that we provide a value for `-o` makes this a build node. 

### Start The Entry Node
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

With our shared memory microslice input source ready, we can start a Flesnet entry node.

```
./flesnet -t zeromq --timeslice-size 20 -n 15 -i 0 -I shm:/fles_in_shared_memory/0 -O shm:/fles_out_shared_memory/0 --processor-instances 1 -e "./tsclient -i shm:%s -o file:timeslice_archive.tsa"
```

- `-i`: The input index of this `flesnet` instance. Example: When `-i` is set to 2, this entry node will read from the 3rd (indexing starts at 0) input source, defined with the `-I` flag. Again, this makes more sense when having seperate entry and build nodes.
- `I`: List of input sources. See use of `-i` flag.

The fact that we provide a value for `-i` makes this an entry node. Using `-i` and `-o` together is also possible but will not be explained in this manual.   

Open a terminal and run the shown command.

As soon as the entry node has started the transmission of microslices from the entry node and the building of timeslices in the build node will start.

After processing the `-n` amount of timeslices (here 15) build and entry node will terminate.

Entry node output: 
```
./flesnet -n 15 -t zeromq -i 0 -I shm:/fles_in_shared_memory/0 -O shm:/fles_out_shared_memory0/0 --processor-instances 1 -e "./tsclient -i shm:%s -o file:timeslice_archive.tsa"
[09:16:52] INFO: this is input 0 (of 1)
[09:16:52] INFO: timeslice size: 100 microslices
[09:16:52] INFO: number of timeslices: 15
[09:16:52] INFO: using shared memory (fles_in_shared_memory)
[09:16:52] INFO: [i0] |#############_______|__________| 0 B/s (0 Hz)
[09:16:53] INFO: [i0] |##########..._______|__________| 14.7 MB/s (1.471 kHz)
[09:16:53] INFO: exiting
```

Build node output:
```
./flesnet -n 15 -t zeromq -o 0 -I shm://127.0.0.1/0 -O shm:/fles_out_shared_memory/0  --processor-instances 1 -e "./tsclient -i shm:%s -o file:timeslice_archive.tsa" 
[09:16:50] INFO: this is output 0 (of 1)
[09:16:50] INFO: timeslice buffer 0: fles_out_shared_memory {43c35bdf-03ab-42a9-ba13-5800a8f6786a}, size: 1 * (128 MiB + 16 MiB) = 144 MiB
[09:16:50] INFO: [c0] |____________________|__________| 
[09:16:50] INFO: worker connected: TimesliceAutoSource at PID 12311 (s1/o0/p0/g0)
[09:16:52] INFO: [c0] |____________________|__________| 
TimesliceReceiver: opened shared memory fles_out_shared_memory {43c35bdf-03ab-42a9-ba13-5800a8f6786a}
[09:16:52] INFO: exiting
[09:16:52] ERROR: ZMQ: Interrupted system call
[09:16:52] INFO: total timeslices processed: 15
```

The output of build and entry node will tell you the amount of processed timeslices.

You will now have a `timeslice_archive.tsa` file, which contains 15 timeslices with a size of 20 microslices each.

## Validate Output Against Input

During development you have to make sure, that the output the build nodes created is valid. That's what the `archive_validator` is for. Together with the known input (*.msa files), the `archive_validator`
can confirm the correctness of the timeslice archives (*.tsa).

Based on the parameters used in the `Running Flesnet` section you can validate the microslice and timeslice archive files with the following command:

```
./archive_validator -I ms_archive.msa -O timeslice_archive.tsa --timeslice-size 20 --overlap 1 --timeslice-cnt 15
```

- `--timeslice-size`: The expected (core) size of timeslices in the *.tsa file(s) 
- `--timeslice-cnt`: The expected summed up amount of timeslices accross all given timeslice archive files.
- `--overlap`: Size of overlap between timeslices. In the `Running Flesnet` example did not explicitly set this parameter, therefore the default value of 1 was used.
- `-I`: Space-seperated list of filepaths pointing to the used microslice archive files. 
- `-O`: Space-seperated list of filepaths pointing to the created timeslice archive files created by Flesnet and its build nodes.

In case of valid archives, the output will look similar to:
```
./archive_validator -I ms_archive.msa -O timeslice_archive.tsa --timeslice-size 20 --overlap 1 --timeslice-cnt 15
[10:07:46] INFO: Verifying metadata of: 'timeslice_archive.tsa' ...
[10:07:46] INFO: Successfully verified metadata of: 'timeslice_archive.tsa'. 0 remaining ...
[10:07:46] INFO: Verified: 
[10:07:46] INFO: overall microslices: 301
[10:07:46] INFO: overlaps: 14
[10:07:46] INFO: overall timeslices: 15
[10:07:46] INFO: Success - Archives valid.
[10:07:46] INFO: exiting
```