#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run flesnet timeslice building benchmarks inside a Slurm allocation.

Combined mode: every data node runs one stserver and one tsbuilder. One
tsmanager runs on the first data node ("shared") or on an extra node of the
allocation ("extra"). Each scenario point (sweep product x repetitions) gets
its own run directory with metadata, logs and per-process metric files
(influx line format, see analyze.py).

Usage (inside salloc/sbatch):
  flesbench.py scenario.yaml [--out DIR] [--dry-run]
"""

import argparse
import copy
import datetime
import itertools
import json
import os
import re
import shlex
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

try:
    import yaml
except ImportError:  # JSON is valid YAML, so JSON scenarios work everywhere
    yaml = None

DEFAULTS = {
    "name": "bench",
    "binaries": None,  # directory with stserver, tsbuilder, tsmanager
    "nodes": None,  # number of data nodes (default: all available)
    "manager": "shared",  # shared | extra
    "control_iface": None,  # e.g. ib0: use its IPv4 address for UCX sockaddr
    "timeslice_duration": "40ms",
    "warmup": "20s",
    "duration": "60s",
    "stop_timeout": "30s",
    "repetitions": 1,
    "pause": "5s",  # between points
    "ports": {"tsmanager": 23373, "stserver": 23374},
    "srun_args": ["--overlap", "--cpu-bind=none"],
    "env": {},
    "stserver": {"env": {}, "prefix": "", "args": {}},
    "tsbuilder": {"env": {}, "prefix": "", "args": {}},
    "tsmanager": {"env": {}, "prefix": "", "args": {}},
    "sweep": {},
}

ROLES = ("tsmanager", "tsbuilder", "stserver")


class Stop(Exception):
    """Raised by the SIGTERM handler so running steps are stopped cleanly."""


def log(msg):
    stamp = datetime.datetime.now().strftime("%H:%M:%S")
    print(f"[{stamp}] {msg}", flush=True)


def parse_duration(value):
    """Return seconds for '40ms', '1.5s', '2min', or a plain number."""
    if isinstance(value, (int, float)):
        return float(value)
    m = re.fullmatch(r"\s*([0-9.]+)\s*(ns|us|ms|s|min|h)?\s*", str(value))
    if not m:
        raise ValueError(f"bad duration: {value!r}")
    scale = {"ns": 1e-9, "us": 1e-6, "ms": 1e-3, "s": 1, "min": 60, "h": 3600}
    return float(m.group(1)) * scale[m.group(2) or "s"]


def merge(base, override):
    out = copy.deepcopy(base)
    for k, v in (override or {}).items():
        if isinstance(v, dict) and isinstance(out.get(k), dict):
            out[k] = merge(out[k], v)
        else:
            out[k] = copy.deepcopy(v)
    return out


def set_dotted(cfg, key, value):
    parts = key.split(".")
    d = cfg
    for p in parts[:-1]:
        d = d.setdefault(p, {})
    d[parts[-1]] = value


def load_scenario(path):
    text = Path(path).read_text()
    data = yaml.safe_load(text) if yaml else json.loads(text)
    cfg = merge(DEFAULTS, data)
    if cfg["binaries"] is None:
        cfg["binaries"] = str(Path(__file__).resolve().parents[2] / "build")
    else:
        b = Path(cfg["binaries"]).expanduser()
        if not b.is_absolute():
            b = Path(path).resolve().parent / b
        cfg["binaries"] = str(b.resolve())
    return cfg


def expand_points(cfg):
    """Yield (label, point_cfg) for the sweep product (or the plain config)."""
    sweep = cfg.get("sweep") or {}
    keys = list(sweep)
    for values in itertools.product(*(sweep[k] for k in keys)):
        point = copy.deepcopy(cfg)
        point.pop("sweep", None)
        parts = []
        for k, v in zip(keys, values):
            set_dotted(point, k, v)
            parts.append(f"{k.split('.')[-1]}={v}")
        label = ",".join(parts) or "base"
        point["point"] = dict(zip(keys, values))
        yield label, point


def slug(text):
    return re.sub(r"[^A-Za-z0-9=.,_-]+", "_", text)[:120]


def allocation_nodes():
    nodelist = os.environ.get("SLURM_JOB_NODELIST")
    if not nodelist:
        sys.exit("error: not inside a Slurm allocation (SLURM_JOB_NODELIST unset)")
    out = subprocess.run(
        ["scontrol", "show", "hostnames", nodelist],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    return out.split()


def assign_roles(cfg, nodes):
    if cfg["manager"] == "extra":
        manager, pool = nodes[0], nodes[1:]
    elif cfg["manager"] == "shared":
        manager, pool = None, nodes
    else:
        sys.exit(f"error: manager must be 'shared' or 'extra', not {cfg['manager']!r}")
    n = cfg["nodes"] or len(pool)
    if n > len(pool):
        sys.exit(f"error: scenario wants {n} data nodes, allocation has {len(pool)}")
    data = pool[:n]
    return (manager or data[0]), data


def srun_base(cfg, nodes):
    n = len(nodes)
    return (
        ["srun", "-N", str(n), "-n", str(n), "--ntasks-per-node=1"]
        + ["-w", ",".join(nodes)]
        + list(cfg["srun_args"])
    )


SYSINFO_SCRIPT = r"""
h=$(hostname -s)
f="$OUT/sysinfo/$h.txt"
{
echo "## uname"; uname -a
echo "## lscpu"; lscpu
echo "## numactl"; numactl -H 2>&1
echo "## memory"; free -b; df -B1 /dev/shm
echo "## ulimit -l"; ulimit -l
echo "## ib devices (sysfs)"
for d in /sys/class/infiniband/*; do
  [ -e "$d" ] || continue
  p=$(readlink -f "$d/device")
  echo "$(basename "$d") pci=$(basename "$p") numa=$(cat "$p/numa_node")" \
       "link='$(cat "$p/current_link_speed") x$(cat "$p/current_link_width")'" \
       "max='$(cat "$p/max_link_speed") x$(cat "$p/max_link_width")'" \
       "netdev=$(ls "$d/device/net" 2>/dev/null | tr '\n' ' ')"
done
echo "## ibstat"; ibstat 2>&1
echo "## ip addr"; ip -o -4 addr
echo "## /dev/shm"; ls -la /dev/shm
} > "$f" 2>&1
addr=""
if [ -n "$IFACE" ]; then
  addr=$(ip -o -4 addr show dev "$IFACE" | awk '{print $4}' | cut -d/ -f1 | head -1)
fi
for d in /sys/class/infiniband/*; do
  [ -e "$d" ] || continue
  p=$(readlink -f "$d/device")
  if [ "$(cat "$p/current_link_width")" != "$(cat "$p/max_link_width")" ]; then
    echo "WARN $h $(basename "$d"): PCIe link x$(cat "$p/current_link_width"), device supports x$(cat "$p/max_link_width")"
  fi
done
echo "NODE $h $addr"
"""


def probe_nodes(cfg, nodes, outdir):
    """Write per-node system info and return {node: control address}."""
    (outdir / "sysinfo").mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, OUT=str(outdir), IFACE=cfg["control_iface"] or "")
    res = subprocess.run(
        srun_base(cfg, nodes) + ["bash", "-c", SYSINFO_SCRIPT],
        env=env,
        capture_output=True,
        text=True,
        stdin=subprocess.DEVNULL,
    )
    addrs = {}
    for line in res.stdout.splitlines():
        parts = line.split()
        if parts and parts[0] == "NODE":
            addrs[parts[1]] = parts[2] if len(parts) > 2 else parts[1]
        elif parts and parts[0] == "WARN":
            log("warning: " + line[5:])
    missing = [n for n in nodes if n not in addrs]
    if res.returncode != 0 or missing:
        sys.exit(f"error: node probe failed (missing {missing}): {res.stderr}")
    if cfg["control_iface"]:
        bad = [n for n, a in addrs.items() if a == n]
        if bad:
            sys.exit(f"error: no address on {cfg['control_iface']} on {bad}")
    return addrs


def run_cleanup(cfg, nodes, shm_names):
    files = " ".join(shlex.quote(f"/dev/shm/{s}") for s in shm_names)
    subprocess.run(
        srun_base(cfg, nodes) + ["bash", "-c", f"rm -f {files}"],
        stdin=subprocess.DEVNULL,
        capture_output=True,
    )


def cli_args(args):
    out = []
    for k, v in args.items():
        if v is None or v is False:
            continue
        out.append(f"--{k}")
        if v is not True:
            out.append(str(v))
    return out


def role_command(cfg, role, args, rundir):
    """Return the node-side shell command for a role.

    $NODE_ADDR is set per node by the wrapper; the metric file and log names
    use the short host name.
    """
    exe = str(Path(cfg["binaries"]) / role)
    prefix = shlex.split(cfg[role].get("prefix") or "")
    metric = f'-m "file:{rundir}/metrics/{role}-$(hostname -s).influx"'
    argstr = " ".join(shlex.quote(a) for a in prefix + [exe] + cli_args(args))
    return f"exec {argstr} {metric}"


NODE_WRAPPER = r"""
h=$(hostname -s)
NODE_ADDR=$h
while read -r n a; do [ "$n" = "$h" ] && NODE_ADDR=$a; done <<'EOF'
{addrs}
EOF
export NODE_ADDR
{cmd}
"""


def start_role(cfg, role, nodes, args, rundir, addrs, extra_cmd_args=""):
    env = dict(os.environ)
    env.update({k: str(v) for k, v in cfg["env"].items()})
    env.update({k: str(v) for k, v in cfg[role].get("env", {}).items()})
    cmd = role_command(cfg, role, args, rundir) + extra_cmd_args
    table = "\n".join(f"{n} {a}" for n, a in addrs.items())
    script = NODE_WRAPPER.replace("{addrs}", table).replace("{cmd}", cmd)
    argv = srun_base(cfg, nodes) + [
        f"--job-name={role}",
        f"--output={rundir}/logs/{role}-%N.log",
        "bash",
        "-c",
        script,
    ]
    (rundir / f"cmd-{role}.sh").write_text(
        "# srun: " + shlex.join(argv[:-3]) + "\n" + script
    )
    proc = subprocess.Popen(argv, env=env, stdin=subprocess.DEVNULL)
    proc.role = role
    return proc


def step_ids(names):
    """Return the Slurm step ids of this job's steps with the given names."""
    out = subprocess.run(
        ["squeue", "-h", "-s", "-j", os.environ["SLURM_JOB_ID"], "-o", "%i %j"],
        capture_output=True,
        text=True,
    ).stdout
    return [sid for sid, name in (l.split(maxsplit=1) for l in out.splitlines()) if name in names]


def stop_steps(procs, timeout):
    """Send SIGTERM to the tasks of the steps, then cancel what is left.

    Signaling srun itself does not work: srun answers SIGTERM by killing the
    step, so the processes would never shut down cleanly.
    """
    names = {p.role for p in procs if p.poll() is None}
    ids = step_ids(names) if names else []
    if ids:
        subprocess.run(["scancel", "-s", "TERM", *ids])
    deadline = time.time() + timeout
    for p in procs:
        try:
            p.wait(max(0.1, deadline - time.time()))
        except subprocess.TimeoutExpired:
            log(f"{p.role} did not stop within {timeout:.0f} s, canceling")
            subprocess.run(["scancel", *step_ids({p.role})])
            p.wait()


def tool_output(argv):
    try:
        return subprocess.run(argv, capture_output=True, text=True, timeout=30).stdout
    except (OSError, subprocess.SubprocessError) as e:
        return f"error: {e}"


def run_point(cfg, label, outdir, manager, data_nodes, addrs, rep):
    rundir = outdir / slug(f"{label}-r{rep}")
    (rundir / "metrics").mkdir(parents=True, exist_ok=True)
    (rundir / "logs").mkdir(exist_ok=True)

    jobid = os.environ.get("SLURM_JOB_ID", "nojob")
    shm_st = f"bench{jobid}_st"
    shm_ts = f"bench{jobid}_ts"
    tsd = cfg["timeslice_duration"]
    mgr_addr = f"{addrs[manager]}:{cfg['ports']['tsmanager']}"

    args = {
        "tsmanager": merge(
            {"listen-port": cfg["ports"]["tsmanager"], "timeslice-duration": tsd},
            cfg["tsmanager"]["args"],
        ),
        "tsbuilder": merge(
            {"tsmanager-address": mgr_addr, "shm-id": shm_ts},
            cfg["tsbuilder"]["args"],
        ),
        "stserver": merge(
            {
                "tsmanager-address": mgr_addr,
                "listen-port": cfg["ports"]["stserver"],
                "shm": shm_st,
                "timeslice-duration": tsd,
            },
            cfg["stserver"]["args"],
        ),
    }
    stserver_extra = ""
    if "advertise-host" not in args["stserver"] and cfg["control_iface"]:
        port = args["stserver"]["listen-port"]
        stserver_extra = f' --advertise-host "$NODE_ADDR:{port}"'

    all_nodes = sorted(set(data_nodes) | {manager})
    meta = {
        "label": label,
        "repetition": rep,
        "point": cfg.get("point", {}),
        "scenario": {k: v for k, v in cfg.items() if k != "point"},
        "args": args,
        "slurm_job_id": jobid,
        "manager": manager,
        "data_nodes": data_nodes,
        "n": len(data_nodes),
        "addresses": addrs,
        "flesnet_version": tool_output(
            [str(Path(cfg["binaries"]) / "tsbuilder"), "--version"]
        ).strip(),
        "ucx_info": tool_output(["ucx_info", "-v"]).strip(),
        "environment": {
            k: v
            for k, v in os.environ.items()
            if k.startswith(("UCX_", "SLURM_JOB", "OMPI_"))
        },
    }
    meta_path = rundir / "meta.json"
    meta_path.write_text(json.dumps(meta, indent=2))

    run_cleanup(cfg, all_nodes, [shm_st, shm_ts])
    procs = []
    try:
        log(f"{rundir.name}: tsmanager on {manager}, {len(data_nodes)} data nodes")
        procs.append(start_role(cfg, "tsmanager", [manager], args["tsmanager"], rundir, addrs))
        time.sleep(1)
        procs.append(start_role(cfg, "tsbuilder", data_nodes, args["tsbuilder"], rundir, addrs))
        procs.append(
            start_role(cfg, "stserver", data_nodes, args["stserver"], rundir, addrs, stserver_extra)
        )
        t_start = time.time_ns()
        warmup, duration = parse_duration(cfg["warmup"]), parse_duration(cfg["duration"])
        end = time.time() + warmup + duration
        while time.time() < end:
            dead = [p for p in procs if p.poll() is not None]
            if dead:
                log(f"a step exited early (rc={dead[0].returncode}), aborting point")
                break
            time.sleep(0.5)
        t_stop = time.time_ns()
    finally:
        # data nodes first, then the manager
        timeout = parse_duration(cfg["stop_timeout"])
        stop_steps(procs[1:], timeout)
        stop_steps(procs[:1], timeout)
        run_cleanup(cfg, all_nodes, [shm_st, shm_ts])

    meta["t_start_ns"] = t_start
    meta["t_stop_ns"] = t_stop
    meta["window_ns"] = [t_start + int(warmup * 1e9), t_stop]
    meta["exit_codes"] = {p.role: p.returncode for p in procs}
    meta["completed"] = t_stop - t_start >= int((warmup + duration) * 1e9) - 10**9
    meta_path.write_text(json.dumps(meta, indent=2))
    return rundir


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("scenario")
    ap.add_argument("--out", default="runs", help="output base directory (default: runs)")
    ap.add_argument("--dry-run", action="store_true", help="list the points and exit")
    opts = ap.parse_args()

    cfg = load_scenario(opts.scenario)
    points = list(expand_points(cfg))
    reps = int(cfg["repetitions"])
    if opts.dry_run:
        for label, _ in points:
            print(f"{label} x{reps}")
        return

    for role in ROLES:
        exe = Path(cfg["binaries"]) / role
        if not os.access(exe, os.X_OK):
            sys.exit(f"error: {exe} not executable")

    nodes = allocation_nodes()
    manager, data_nodes = assign_roles(cfg, nodes)
    jobid = os.environ.get("SLURM_JOB_ID", "nojob")
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    outdir = Path(opts.out).resolve() / f"{stamp}-{slug(cfg['name'])}-j{jobid}"
    outdir.mkdir(parents=True)
    shutil.copy(opts.scenario, outdir / "scenario.yaml")

    addrs = probe_nodes(cfg, sorted(set(data_nodes) | {manager}), outdir)
    log(f"campaign {outdir}: {len(points)} point(s) x {reps}, nodes {data_nodes}")

    def on_term(*_):
        raise Stop()

    signal.signal(signal.SIGTERM, on_term)
    try:
        for label, point in points:
            for rep in range(reps):
                run_point(point, label, outdir, manager, data_nodes, addrs, rep)
                time.sleep(parse_duration(cfg["pause"]))
    except (Stop, KeyboardInterrupt):
        log("interrupted")
        sys.exit(1)
    log(f"done: {outdir}")


if __name__ == "__main__":
    main()
