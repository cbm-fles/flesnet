#!/usr/bin/env python3
"""Start readout on a node."""

import glob
import os
import subprocess
import signal
import sys
import time
import argparse

import flescfg


# Global parameters, may be overwritten by environment
FLESNETDIR = os.getenv("FLESNETDIR", "/usr/bin/")
SPMDIR = os.getenv("SPMDIR", "/opt/spm/")
LOGDIR = os.getenv("LOGDIR", "log/")
SHM_PREFIX = os.getenv("SHM_PREFIX", "readout_")


# Global variables
processes: list[subprocess.Popen] = []  # Spawned processes
pgen_in_use: bool = False
MAY_END: bool = False
END_REQUESTED: bool = False


def cleanup_shm():
    """Remove all shared memory files."""
    print("Cleaning up shm files...")
    for shm_file in glob.glob(f"/dev/shm/{SHM_PREFIX}*"):
        os.remove(shm_file)


def term_subprocesses(timeout=10) -> bool:
    """Send SIGTERM to all subprocesses and wait for them to exit.
    If any remain, return False."""
    print(f"Sending SIGTERM to all {len(processes)} subprocesses...")
    for process in processes:
        if process.poll() is None:  # Process is still running
            os.killpg(os.getpgid(process.pid), signal.SIGTERM)

    # Wait for all processes to terminate or timeout
    start_time = time.time()
    while time.time() - start_time < timeout:
        all_terminated = True
        for process in processes:
            if process.poll() is None:  # Process is still running
                all_terminated = False
                break
        if all_terminated:
            print("All subprocesses terminated gracefully.")
            return True
        time.sleep(0.5)  # Check every 500ms
    return False


def kill_subprocesses() -> None:
    """Send SIGKILL to all subprocesses."""
    print("Sending SIGKILL to remaining subprocesses...")
    for process in processes:
        if process.poll() is None:  # Process is still running
            os.killpg(os.getpgid(process.pid), signal.SIGKILL)


def end_readout():
    """End readout by shutting down all subprocesses."""
    print("Shutting down...")
    if pgen_in_use:
        print("Disabling pattern generators...")
        subprocess.run(
            [os.path.join(FLESNETDIR, "cri_en_pgen"), "0"],
            stdout=open(LOGDIR + "cri_en_pgen.log", "a", encoding="utf-8"),
            check=False,
        )
    if term_subprocesses():
        print("Exiting")
        sys.exit(0)
    else:
        print("Some subprocesses did not terminate gracefully.")
        kill_subprocesses()
        cleanup_shm()
        sys.exit(1)


def handle_signal(signum, _):
    """Handle SIGINT or SIGTERM signal by initiating end of readout."""
    signal_name = signal.Signals(signum).name
    print(f"Received signal {signal_name}.")
    if MAY_END:
        end_readout()
    else:
        print("Requesting end of readout.")
        global END_REQUESTED  # pylint: disable=global-statement
        END_REQUESTED = True


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
    subprocess.run(
        [os.path.join(FLESNETDIR, "cri_info")],
        stdout=open(LOGDIR + "cri_info.log", "w", encoding="utf-8"),
        check=False,
    )

    use_pgen = False
    common = config["common"]
    cards = config["entry_nodes"][hostname]["cards"]

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
            f"{common['pgen_mc_size_ns'] // 1000}",
            "-r",
            f"{common['pgen_rate']}",
            "--mc-size-limit",
            f"{common['mc_size_limit_bytes']}",
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
        subprocess.run(cmd, check=False)
    global pgen_in_use  # pylint: disable=global-statement
    pgen_in_use = use_pgen

    # Start cri_server subprocesses, each in its own process group
    print("Starting cri_server instance(s)...")
    for card, cardinfo in cards.items():
        readout_buffer_size = cardinfo.get(
            "default_readout_buffer_size", common["default_readout_buffer_size"]
        )
        # Find the smallest power of 2 that is greater or equal to the buffer size
        readout_buffer_size_exp = 0
        while (1 << readout_buffer_size_exp) < readout_buffer_size:
            readout_buffer_size_exp += 1
        cmd = [
            os.path.join(FLESNETDIR, "cri_server"),
            "-c",
            "/dev/null",
            "-L",
            f"{LOGDIR}{card}_server.log",
            "--log-syslog",
            "-i",
            f"{cardinfo['pci_address']}",
            "--archivable-data=false",
            f"--data-buffer-size-exp={readout_buffer_size_exp}",
            "-o",
            f"{SHM_PREFIX}{card}",
            "-e",
            f"{os.path.join(SPMDIR, "spm-provide")} cri_server_sem",
        ]
        process = subprocess.Popen(cmd, start_new_session=True)
        processes.append(process)

    # Block until servers are ready
    print(f"Waiting for {len(cards)} cri_server instance(s)...")
    subprocess.run(
        [os.path.join(SPMDIR, "spm-require"), f"-n{len(cards)}", "cri_server_sem"],
        check=False,
    )
    print("... all cri_server(s) started")

    # First opportunity to safely shutdown
    if END_REQUESTED:
        end_readout()

    time.sleep(1)
    if pgen_in_use:
        print("Enabling pattern generators...")
        subprocess.run(
            [FLESNETDIR + "cri_en_pgen", "1"],
            stdout=open(LOGDIR + "cri_en_pgen.log", "w", encoding="utf-8"),
            check=False,
        )

    # From this point it is safe to shut down any time
    global MAY_END  # pylint: disable=global-statement
    MAY_END = True
    # Check for race conditions
    if END_REQUESTED:
        end_readout()

    subprocess.run([os.path.join(SPMDIR, "spm-provide"), "fles_input_sem"], check=False)
    print("Running...")
    # Monitor all subprocesses
    while processes:
        for process in processes[:]:
            retcode = process.poll()  # Check if the process has terminated
            if retcode is not None:  # Process has finished
                print(f"Subprocess {process.pid} finished with return code {retcode}")
                processes.remove(process)
        time.sleep(0.5)  # Avoid busy-waiting

    print("*** YOU SHOULD NEVER SEE THIS ***")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Start readout on a node.")
    parser.add_argument("config", help="Path to the configuration file")
    parser.add_argument("hostname", help="Hostname of the node")
    args = parser.parse_args()
    main(args.config, args.hostname)
