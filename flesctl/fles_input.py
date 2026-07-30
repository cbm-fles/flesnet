#!/usr/bin/env python3
"""Start readout on a node."""

import os
import subprocess
import signal
import sys
import time
import argparse

import flescfg


# Global parameters, may be overwritten by environment
FLESNETDIR = os.getenv("FLESNETDIR", "/usr/bin/")
LOGDIR = os.getenv("LOGDIR", "log/")
STSERVER_CFG = os.getenv("STSERVER_CFG", "stserver.cfg")


# Global variables
stserver: subprocess.Popen | None = None  # The spawned stserver process
pgen_in_use: bool = False


def stop_stserver(timeout=10) -> bool:
    """Stop the stserver, forcefully if it does not react in time."""
    if stserver is None or stserver.poll() is not None:
        return True
    print("Sending SIGTERM to stserver...")
    os.killpg(os.getpgid(stserver.pid), signal.SIGTERM)
    try:
        stserver.wait(timeout=timeout)
        print("stserver terminated gracefully.")
        return True
    except subprocess.TimeoutExpired:
        print("stserver did not terminate gracefully, sending SIGKILL...")
        os.killpg(os.getpgid(stserver.pid), signal.SIGKILL)
        return False


def disable_pgen() -> None:
    """Disable the CRI pattern generators if they are in use."""
    if not pgen_in_use:
        return
    print("Disabling pattern generators...")
    subprocess.run(
        [os.path.join(FLESNETDIR, "cri_en_pgen"), "0"],
        stdout=open(LOGDIR + "cri_en_pgen.log", "a", encoding="utf-8"),
        check=False,
    )


def end_readout():
    """End readout by shutting the stserver down."""
    print("Shutting down...")
    disable_pgen()
    ok = stop_stserver()
    print("Exiting")
    sys.exit(0 if ok else 1)


def handle_signal(signum, _):
    """Handle SIGINT or SIGTERM signal by initiating end of readout."""
    print(f"Received signal {signal.Signals(signum).name}.")
    end_readout()


def main(config_file: str, hostname: str):
    """Start readout on a node."""
    # load the config file
    config = flescfg.load(config_file)
    if config is None:
        print("Error loading configuration file.")
        sys.exit(1)

    # handle end signals
    for sig in [signal.SIGINT, signal.SIGTERM, signal.SIGHUP]:
        signal.signal(sig, handle_signal)

    print("Starting readout...")

    use_pgen = False
    nodeinfo = config["entry_nodes"][hostname]
    common = config["common"]
    cri = common["cri"]
    cards = nodeinfo["cards"]

    # a node without cards runs the software pattern generator of its stserver
    # only, so there is no CRI to inspect and configure
    if cards:
        # diagnostic only, its output is not used further
        subprocess.run(
            [os.path.join(FLESNETDIR, "cri_info")],
            stdout=open(LOGDIR + "cri_info.log", "w", encoding="utf-8"),
            check=False,
        )
        print("Configuring CRIs...")
    for card, cardinfo in cards.items():
        cmd = [
            os.path.join(FLESNETDIR, "cri_cfg"),
            "-l",
            "2",
            "-L",
            f"{LOGDIR}{card}_cfg.log",
            "-i",
            f"{cardinfo['pci_address']}",
            "-t",
            f"{cri['pgen_mc_size_ns'] // 1000}",
            "-r",
            f"{cri['pgen_rate']}",
            "--mc-size-limit",
            f"{cri['mc_size_limit_bytes']}",
        ]
        channels = cardinfo["channels"]
        for channel, channelinfo in channels.items():
            mode = channelinfo["mode"]
            cmd += [f"--c{channel}_source", f"{mode}"]
            if mode == "pgen":
                cmd += [
                    f"--c{channel}_eq_id",
                    f"{cardinfo['pgen_base_eqid'] + channel}",
                ]
                use_pgen = True
        # the stserver determines the set of enabled channels once at startup,
        # so a failed configuration cannot be recovered from later in the run
        if subprocess.run(cmd, check=False).returncode != 0:
            print(f"Error: cri_cfg failed for card {card}, aborting.")
            sys.exit(1)
    global pgen_in_use  # pylint: disable=global-statement
    pgen_in_use = use_pgen

    # A single stserver instance takes care of all CRIs of this node. It has to
    # be started after cri_cfg, as it determines the set of enabled channels
    # once at startup.
    print("Starting stserver...")
    env = os.environ.copy()
    if "ucx_net_devices" in nodeinfo:
        env["UCX_NET_DEVICES"] = nodeinfo["ucx_net_devices"]
    cmd = [
        os.path.join(FLESNETDIR, "stserver"),
        "-c",
        STSERVER_CFG,
        "-L",
        f"{LOGDIR}stserver.log",
        "--log-syslog",
        "--advertise-host",
        f"{nodeinfo['address']}",
    ]
    global stserver  # pylint: disable=global-statement
    stserver = subprocess.Popen(cmd, env=env, start_new_session=True)

    # The data path is established asynchronously, so the pattern generators
    # can be enabled right away; data produced before the stserver is ready is
    # dropped at the CRI.
    if pgen_in_use:
        print("Enabling pattern generators...")
        subprocess.run(
            [os.path.join(FLESNETDIR, "cri_en_pgen"), "1"],
            stdout=open(LOGDIR + "cri_en_pgen.log", "w", encoding="utf-8"),
            check=False,
        )

    print("Running...")
    # Poll instead of waiting: a blocking wait holds the lock that the wait in
    # stop_stserver needs when a signal arrives, which would keep the handler
    # from reaping the process.
    while stserver.poll() is None:
        time.sleep(0.5)
    print(f"Error: stserver exited unexpectedly with return code {stserver.returncode}")
    disable_pgen()
    sys.exit(stserver.returncode)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Start readout on a node.")
    parser.add_argument("config", help="Path to the configuration file")
    parser.add_argument("hostname", help="Hostname of the node")
    args = parser.parse_args()
    main(args.config, args.hostname)
