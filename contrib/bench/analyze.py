#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Summarize flesbench run directories.

For each run directory (one containing meta.json), read the metric files in
metrics/ and compute steady-state rates and loss counters over the measurement
window recorded by flesbench.py (warmup discarded). Writes summary.json into
every run directory and prints one table row per run. With --csv, also writes
all rows to a CSV file.

Usage:
  analyze.py RUN_OR_CAMPAIGN_DIR... [--csv FILE]
"""

import argparse
import csv
import json
import re
import sys
from pathlib import Path

GB = 1e9

# counters whose increase within the window means the point is not sustainable
LOSS_FIELDS = {
    "stserver_status": [
        "pgen_skipped_microslices",
        "subtimeslice_retracted_count",
        "aggregation_allocation_failures",
        "timeslice_incomplete_count",
    ],
    "tsbuilder_status": ["timeslice_incomplete_count"],
    "tsmanager_status": [
        "timeslice_discarded_count",
        "timeslice_incomplete_count",
        "timeslice_reassigned_count",
        "late_announcement_count",
    ],
}

LINE_RE = re.compile(r"^([^, ]+)((?:,[^ ]+)?) (.+) (\d+)$")
FIELD_RE = re.compile(r'([^=,]+)=("(?:[^"\\]|\\.)*"|[^,]+)')


def parse_value(raw):
    if raw.startswith('"'):
        return raw[1:-1]
    if raw.endswith("i"):
        return int(raw[:-1])
    if raw in ("true", "false"):
        return raw == "true"
    return float(raw)


def read_metrics(path):
    """Return {measurement: [(t_ns, fields)]} from an influx line file."""
    series = {}
    for line in path.read_text(errors="replace").splitlines():
        m = LINE_RE.match(line)
        if not m:
            continue
        fields = {k: parse_value(v) for k, v in FIELD_RE.findall(m.group(3))}
        series.setdefault(m.group(1), []).append((int(m.group(4)), fields))
    for samples in series.values():
        samples.sort(key=lambda s: s[0])
    return series


def window_samples(samples, t0, t1):
    return [s for s in samples if t0 <= s[0] <= t1]


def delta(samples, field):
    """Increase of a counter between the first and last sample, and the time."""
    have = [(t, f[field]) for t, f in samples if field in f]
    if len(have) < 2:
        return None, None
    return have[-1][1] - have[0][1], (have[-1][0] - have[0][0]) / 1e9


def last_increase(samples, field):
    """Time (ns) of the last sample at which a counter increased, or None."""
    last, prev = None, None
    for t, f in samples:
        v = f.get(field)
        if v is not None and prev is not None and v > prev:
            last = t
        prev = v if v is not None else prev
    return last


def rate(samples, field):
    d, dt = delta(samples, field)
    return d / dt if d is not None and dt > 0 else None


def analyze_run(rundir):
    meta = json.loads((rundir / "meta.json").read_text())
    if "window_ns" not in meta:
        return None
    t0, t1 = meta["window_ns"]
    n = meta["n"]
    res = {
        "run": rundir.name,
        "label": meta["label"],
        "rep": meta["repetition"],
        "n": n,
        "window_s": round((t1 - t0) / 1e9, 1),
        "completed": meta.get("completed", False),
        **{f"p:{k}": v for k, v in meta.get("point", {}).items()},
    }
    losses = {}
    built, sent, ts_rates = [], [], []
    build_time_mean, build_time_max, delay_mean, delay_max = [], [], [], []
    missing = []
    last_loss = None

    for role, measurement in (
        ("tsbuilder", "tsbuilder_status"),
        ("stserver", "stserver_status"),
        ("tsmanager", "tsmanager_status"),
    ):
        files = sorted((rundir / "metrics").glob(f"{role}-*.influx"))
        expected = 1 if role == "tsmanager" else n
        if len(files) < expected:
            missing.append(f"{role}:{len(files)}/{expected}")
        for f in files:
            samples = window_samples(read_metrics(f).get(measurement, []), t0, t1)
            if len(samples) < 2:
                missing.append(f.stem)
                continue
            for field in LOSS_FIELDS[measurement]:
                d, _ = delta(samples, field)
                if d:
                    key = f"{role}.{field}"
                    losses[key] = losses.get(key, 0) + d
                    t = last_increase(samples, field)
                    last_loss = max(last_loss or 0, t or 0)
            if role == "tsbuilder":
                built.append(rate(samples, "byte_count"))
                ts_rates.append(rate(samples, "timeslice_count"))
                pub = [(s[1]["published_count"], s[1]) for s in samples if "published_count" in s[1]]
                if pub:
                    total = sum(c for c, _ in pub)
                    build_time_mean.append(sum(c * f["build_time_mean_ns"] for c, f in pub) / total)
                    delay_mean.append(sum(c * f["delay_mean_ns"] for c, f in pub) / total)
                    build_time_max.append(max(f["build_time_max_ns"] for _, f in pub))
                    delay_max.append(max(f["delay_max_ns"] for _, f in pub))
            elif role == "stserver":
                sent.append(rate(samples, "data_bytes"))
            else:
                res["tsmanager_ts_per_s"] = rate(samples, "timeslice_count")

    built = [b for b in built if b is not None]
    sent = [s for s in sent if s is not None]
    if built:
        res["built_total_GBps"] = sum(built) / GB
        res["built_per_node_GBps"] = sum(built) / len(built) / GB
        res["built_min_node_GBps"] = min(built) / GB
        # 1/n of every timeslice is the node's own contribution
        res["net_per_node_GBps"] = res["built_per_node_GBps"] * (n - 1) / n
        res["ts_per_s"] = sum(r for r in ts_rates if r is not None)
    if sent:
        res["input_per_node_GBps"] = sum(sent) / len(sent) / GB
    if build_time_mean:
        res["build_time_mean_ms"] = sum(build_time_mean) / len(build_time_mean) / 1e6
        res["build_time_max_ms"] = max(build_time_max) / 1e6
        res["delay_mean_ms"] = sum(delay_mean) / len(delay_mean) / 1e6
        res["delay_max_ms"] = max(delay_max) / 1e6
    res["losses"] = losses
    # losses only early in the window point to a too short warmup
    res["last_loss_s"] = round((last_loss - t0) / 1e9, 1) if last_loss else None
    res["missing"] = missing
    res["sustainable"] = bool(res["completed"] and not losses and not missing and built)
    (rundir / "summary.json").write_text(json.dumps(res, indent=2))
    return res


def find_runs(paths):
    for p in map(Path, paths):
        if (p / "meta.json").exists():
            yield p
        else:
            yield from sorted(m.parent for m in p.glob("*/meta.json"))


def fmt(v, width=8):
    if v is None:
        return "-".rjust(width)
    if isinstance(v, float):
        return f"{v:{width}.2f}"
    return str(v).rjust(width)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("paths", nargs="+")
    ap.add_argument("--csv", help="write all rows to this CSV file")
    opts = ap.parse_args()

    rows = [r for r in map(analyze_run, find_runs(opts.paths)) if r]
    if not rows:
        sys.exit("no completed runs found")

    cols = [
        ("built/node", "built_per_node_GBps"),
        ("net/node", "net_per_node_GBps"),
        ("input/node", "input_per_node_GBps"),
        ("TS/s", "ts_per_s"),
        ("build ms", "build_time_mean_ms"),
        ("max ms", "build_time_max_ms"),
        ("last loss", "last_loss_s"),
    ]
    print(f"{'run':40} {'n':>3} " + " ".join(f"{c:>10}" for c, _ in cols) + "  ok")
    for r in rows:
        ok = "yes" if r["sustainable"] else "NO " + ",".join(
            [f"{k}+{v}" for k, v in r["losses"].items()] + r["missing"]
        )
        print(
            f"{r['run'][:40]:40} {r['n']:>3} "
            + " ".join(fmt(r.get(k), 10) for _, k in cols)
            + f"  {ok}"
        )

    if opts.csv:
        keys = []
        for r in rows:
            keys += [k for k in r if k not in keys]
        with open(opts.csv, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=keys)
            w.writeheader()
            for r in rows:
                w.writerow({k: json.dumps(v) if isinstance(v, (dict, list)) else v for k, v in r.items()})


if __name__ == "__main__":
    main()
