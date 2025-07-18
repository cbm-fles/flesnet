Flesnet software dependencies
=============================

### Linux Distributions

- Ubuntu 16.04 LTS "Xenial Xerus", EOL: 2026-04
- Ubuntu 18.04 LTS "Bionic Beaver", EOL: 2028-04
- Ubuntu 20.04 LTS "Focal Forsa", EOL: 2030-04
- Debian 12 "Bookworm" 23.07, EOL: 2026-07
- Rocky Linux 8.10 "Green Obsidian, EOL: 2029-04
- Ubuntu 22.04 LTS "Jammy Jellyfish", EOL: 2032-04
- Ubuntu 24.04 LTS "Noble Numbat", EOL: 2034-04

Deprecated
- Debian 8 "Jessie", released: 2015-04, EOL: 2022-06
- Debian 9 "Stretch", EOL: 2022-06 (2024-06 with LTS)
- Debian 10 "Buster" 19.07, EOL: 2022-08
- Debian 11 "Bullseye" 21.08, EOL: 2024-07

### Minimum Versions to Support

We (should) support the typical OSs Flesnet is run on. This includes systems at
mCBM, GSI/FAIR, github CI and ZIB.

The following list helps to find minimal versions we should support.

|            | Bookworm | Rocky 8.10 | Ubuntu 22 | Ubuntu 24           |
|            | @ZIB     | @virgo     | @githubCI | @githubCI, @cbmfles |
|------------|----------|------------|-----------|---------------------|
| boost      | 1.74.0   |            | 1.74.0    | 1.83.0              |
| clang      | 14.0.6   |            | 14.0      | 18.1.3              |
| cmake      | 3.25.1   |            | 3.22.1    | 3.28.3              |
| gcc        | 12.2.0   |            | 11.2      | 13.3.0              |
| libibverbs | 44.0     |            | 39.0      | 50.0                |
| libfabric  | 1.17     |            | 1.11      | 1.17                |
| libzmq     | 4.3.4    |            | 4.3.4     | 4.3.5               |


### Debian 8 "Jessie"

- cmake 3.0.2
- gcc 4.9.2
- clang 3.5
- boost 1.55.0.2
- zmq 4.0.5
