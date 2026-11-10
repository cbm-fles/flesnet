#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Measure the MPI/UCX reference bandwidths inside a Slurm allocation.

1. Health check: osu_bw for each neighbor pair (i -> i+1, ring) at one size.
   Pairs far below the median point to a bad node (PCIe width, cable, NUMA).
2. osu_bibw for the first pair over the size range.
3. osu_alltoall with one rank per node over the size range. Network
   throughput per node = (N-1)*m/t, built equivalent = N*m/t, comparable with
   net_per_node and built_per_node from analyze.py.

Usage (inside salloc/sbatch):
  reference.py --osu DIR [--out DIR] [--nodes N] [--env UCX_X=y ...]
"""

import argparse
import datetime
import json
import os
import statistics
import subprocess
import sys
from pathlib import Path

GB = 1e9


def size_arg(text):
    units = {"k": 2**10, "K": 2**10, "M": 2**20, "G": 2**30}
    if text[-1] in units:
        return int(float(text[:-1]) * units[text[-1]])
    return int(text)


def nodes_from_slurm():
    nodelist = os.environ.get("SLURM_JOB_NODELIST")
    if not nodelist:
        sys.exit("error: not inside a Slurm allocation (SLURM_JOB_NODELIST unset)")
    out = subprocess.run(
        ["scontrol", "show", "hostnames", nodelist], check=True, capture_output=True, text=True
    ).stdout
    return out.split()


def find_osu(osu_dir, name):
    hits = sorted(Path(osu_dir).expanduser().rglob(name))
    if not hits:
        sys.exit(f"error: {name} not found below {osu_dir}")
    return str(hits[0])


def run_osu(opts, hosts, binary, args, raw_file):
    argv = ["mpirun", "-np", str(len(hosts)), "-H", ",".join(hosts), "--map-by", "node"]
    argv += ["--bind-to", "none", "--mca", "pml", "ucx"]
    for e in opts.env:
        argv += ["-x", e]
    argv += opts.mpirun_args + [binary] + args
    res = subprocess.run(argv, capture_output=True, text=True)
    raw_file.write_text(" ".join(argv) + "\n" + res.stdout + res.stderr)
    if res.returncode != 0:
        sys.exit(f"error: {Path(binary).name} failed, see {raw_file}")
    rows = []
    for line in res.stdout.splitlines():
        parts = line.split()
        if len(parts) == 2 and not line.startswith("#"):
            rows.append((int(parts[0]), float(parts[1])))
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--osu", required=True, help="OSU micro-benchmarks install directory")
    ap.add_argument("--out", default="runs", help="output base directory (default: runs)")
    ap.add_argument("--nodes", type=int, help="number of nodes (default: all)")
    ap.add_argument("--min-size", type=size_arg, default=size_arg("1M"))
    ap.add_argument("--max-size", type=size_arg, default=size_arg("512M"))
    ap.add_argument("--health-size", type=size_arg, default=size_arg("64M"))
    ap.add_argument("--iterations", type=int, default=20)
    ap.add_argument("--warmup", type=int, default=5)
    ap.add_argument("--env", action="append", default=[], help="VAR=value for all ranks")
    ap.add_argument("--mpirun-args", nargs=argparse.REMAINDER, default=[])
    ap.add_argument("--name", default="reference")
    opts = ap.parse_args()

    nodes = nodes_from_slurm()[: opts.nodes]
    n = len(nodes)
    if n < 2:
        sys.exit("error: need at least two nodes")
    for e in opts.env:
        k, _, v = e.partition("=")
        os.environ[k] = v  # mpirun -x VAR exports the value from here
    opts.env = [e.partition("=")[0] for e in opts.env]

    jobid = os.environ.get("SLURM_JOB_ID", "nojob")
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    outdir = Path(opts.out).resolve() / f"{stamp}-{opts.name}-j{jobid}"
    (outdir / "raw").mkdir(parents=True)
    common = ["-i", str(opts.iterations), "-x", str(opts.warmup)]
    sizes = ["-m", f"{opts.min_size}:{opts.max_size}"]
    osu_bw = find_osu(opts.osu, "osu_bw")
    result = {
        "nodes": nodes,
        "n": n,
        "slurm_job_id": jobid,
        "env": {k: os.environ.get(k) for k in opts.env},
        "ucx_info": subprocess.run(["ucx_info", "-v"], capture_output=True, text=True).stdout.strip(),
    }

    # 1. health check: unidirectional pair bandwidth around the ring
    health = {}
    hs = ["-m", f"{opts.health_size}:{opts.health_size}"]
    for i in range(n if n > 2 else 1):
        pair = [nodes[i], nodes[(i + 1) % n]]
        rows = run_osu(opts, pair, osu_bw, hs + common, outdir / "raw" / f"bw-{pair[0]}-{pair[1]}.txt")
        health[f"{pair[0]}->{pair[1]}"] = rows[-1][1] * 1e6 / GB  # osu reports MB/s
    median = statistics.median(health.values())
    result["health_GBps"] = health
    print(f"pair bandwidth at {opts.health_size / 2**20:.0f} MiB (GB/s):")
    for pair, bw in health.items():
        flag = "  <-- LOW" if bw < 0.9 * median else ""
        print(f"  {pair:24} {bw:7.2f}{flag}")

    # 2. bidirectional bandwidth of the first pair
    rows = run_osu(opts, nodes[:2], find_osu(opts.osu, "osu_bibw"), sizes + common, outdir / "raw" / "bibw.txt")
    result["bibw"] = [{"size": s, "total_GBps": bw * 1e6 / GB} for s, bw in rows]

    # 3. all-to-all, one rank per node
    rows = run_osu(opts, nodes, find_osu(opts.osu, "osu_alltoall"), sizes + common, outdir / "raw" / "alltoall.txt")
    result["alltoall"] = [
        {
            "size": s,
            "latency_us": t,
            "net_per_node_GBps": (n - 1) * s / (t * 1e-6) / GB,
            "built_per_node_GBps": n * s / (t * 1e-6) / GB,
        }
        for s, t in rows
    ]

    print(f"\n{'size':>10} {'bibw total':>11} {'a2a net/node':>13} {'a2a built/node':>15}  (GB/s, n={n})")
    bibw = {r["size"]: r["total_GBps"] for r in result["bibw"]}
    for r in result["alltoall"]:
        b = bibw.get(r["size"])
        print(
            f"{r['size'] / 2**20:8.0f}Mi {b if b is not None else float('nan'):11.2f}"
            f" {r['net_per_node_GBps']:13.2f} {r['built_per_node_GBps']:15.2f}"
        )
    (outdir / "reference.json").write_text(json.dumps(result, indent=2))
    print(f"\nwritten: {outdir}")


if __name__ == "__main__":
    main()
