# Documentation

This folder contains documentation for the Flesnet project. Part of the documentation is generated automatically by Doxygen. The rest of the documentation is written in Markdown and can be found in this folder. The `doc/archive` folder contains older documentation that is no longer maintained but may still be useful for reference.

## Key documentation

- [`doc/INSTALL.md`](./INSTALL.md) — installation and setup instructions for Flesnet.
- [`doc/FAQ.md`](./FAQ.md) — common issues and frequently asked questions.


## Doxygen documentation

To generate the Doxygen documentation run `make doc` from inside the build directory: 
```
mkdir build && cd build && cmake ..
make doc
```

You can find the generated documentation in `<project>/build/doc`. Doxygen generates separate documentation for each
executable within the project. For Debian, if you have the `php-cli` package installed, you can serve the HTML
documentation (for e.g. tsclient executable) locally by executing:
```
cd build/doc/tsclient-doc/html
php -S localhost:8181
```

Open your browser and visit [http://localhost:8181](http://localhost:8181).

Since the current state of the documentation is limited, you can also instruct doxygen to automatically generate
documentation for undocumented code by setting the cmake variable `EXTENDED_DOXYGEN_DOCUMENTATION` to `YES` (default is
`NO`) on the command line when running cmake (i.e `cmake -DEXTENDED_DOXYGEN_DOCUMENTATION=YES ..`).  This will set the
doxygen option `EXTRACT_ALL` to `YES` as well as instruct doxygen to show all files and generate call and caller graphs
for all functions and methods. Due to the large number of graphs generated, the default (of doxygen) is to generate
these graphs in parallel, regardless of whether `make doc` is called with the option `-j` or not. This can be changed by
setting the cmake variable `DOXYGEN_DOT_NUM_THREADS` to `1` (default is `0`, i.e. to use an appropriate number of
threads) on the command line when running cmake (i.e `cmake -DEXTENDED_DOCUMENTATION=YES -DDOXYGEN_DOT_NUM_THREADS=1
..`).

By default, generation of the documentation with doxygen is silenced. To see the output of doxygen, set the cmake
variable `SILENCE_DOXYGEN` to `NO` (default is `YES`). However, even then warnings about undocumented code are still
suppressed unless `DOXYGEN_WARN_IF_UNDOCUMENTED` is set to `YES` (default is `NO`).

Some doxygen warnings (and one error) are expected and can be regarded as unresolved issues described in more detail in
`doc/CMakelists.txt`.