# Timeslice building benchmarks

Scripts to measure stserver → tsbuilder throughput under Slurm.

- `flesbench.py SCENARIO [--out DIR]`: run inside an allocation. Runs every
  point of the scenario's sweep (times `repetitions`) and creates one run
  directory per point with `meta.json`, `sysinfo/`, `logs/` and `metrics/`
  (one influx line file per process).
- `analyze.py DIR... [--csv FILE]`: per-run steady-state rates and loss
  counters over the measurement window (warmup discarded); writes
  `summary.json` per run. A point is *sustainable* only if no skip, retract,
  discard, incomplete or late counter increases inside the window.
- `reference.py --osu DIR`: MPI/UCX reference in the same allocation
  (OSU micro-benchmarks): `osu_bw` health check around the node ring,
  `osu_bibw` for one pair, `osu_alltoall` with one rank per node, reported as
  network and built throughput per node.
- `campaign.sbatch`: `sbatch -N 4 campaign.sbatch scenarios/dev-combined.yaml`.

Combined mode: every data node runs one stserver and one tsbuilder; the
tsmanager runs on the first data node (`manager: shared`) or on an extra node
(`manager: extra`). With `control_iface` set, the stserver advertise addresses
and the tsmanager address are the IPv4 addresses of that interface (needed
when `UCX_NET_DEVICES` excludes the Ethernet interface).

Interactive use on the dev cluster:

    salloc -N4 -w node[8-11] --exclusive --mem=0 -t 60 \
      contrib/bench/flesbench.py contrib/bench/scenarios/dev-combined.yaml --out ~/flesbench/runs
    contrib/bench/analyze.py ~/flesbench/runs/<campaign>

Built throughput counts whole timeslices per builder; network throughput is
(N-1)/N of it, because one contribution of every timeslice is local.
