#!/usr/bin/env python3

import glob
import os
import subprocess
import signal
import sys
import time
import argparse
import flescfg

# Global parameters, may be overwritten by environment
FLESNETDIR = os.getenv('FLESNETDIR', '/usr/bin/')
SPMDIR = os.getenv('SPMDIR', '/opt/spm/')
LOGDIR = os.getenv('LOGDIR', 'log/')
SHM_PREFIX = os.getenv('SHM_PREFIX', 'cri_')

# Command line parameters
parser = argparse.ArgumentParser(description='Start Flesnet input processes.')
parser.add_argument('config', help='Path to the configuration file')
parser.add_argument('hostname', help='Hostname')
args = parser.parse_args()

config_file = args.config
hostname = args.hostname

# load the config file
config = flescfg.load(config_file)
if config is None:
    print("Error loading configuration file.")
    exit(1)
    
# Spawned processes
processes = []

def cleanup_shm():
    print("Cleaning up shm files...")
    for shm_file in glob.glob('/dev/shm/{}*'.format(SHM_PREFIX)):
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
    print("Sending SIGKILL to remaining subprocesses...")
    for process in processes:
        if process.poll() is None:  # Process is still running
            os.killpg(os.getpgid(process.pid), signal.SIGKILL)

def end_readout():
    print("Shutting down...")
    if pgen_in_use:
        print("Disabling pattern generators...")
        subprocess.run([os.path.join(FLESNETDIR, 'cri_en_pgen'), '0'],
                       stdout=open(LOGDIR + 'cri_en_pgen.log', 'a'))
    if term_subprocesses():
        print("Exiting")
        sys.exit(0)
    else:
        print("Some subprocesses did not terminate gracefully.")
        kill_subprocesses()
        cleanup_shm()
        sys.exit(1)

MAY_END = False
END_REQUESTED = False

def handle_signal(signum, frame):
    """Handle SIGINT or SIGTERM signal by initiating end of readout."""
    signal_name = signal.Signals(signum).name
    print(f"Received signal {signal_name}.")
    global MAY_END, END_REQUESTED
    if MAY_END:
        end_readout()
    else:
        print("Requesting end of readout.")
        END_REQUESTED = True

# handle end signals
for sig in [signal.SIGINT, signal.SIGTERM, signal.SIGHUP]:
    signal.signal(sig, handle_signal)

print("Starting readout...")
subprocess.run([os.path.join(FLESNETDIR, "cri_info")],
               stdout=open(LOGDIR + "cri_info.log", "w"))

pgen_in_use = False
common = config['common']
cards = config['entry_nodes'][hostname]['cards']

print("Configuring CRIs...")
for card in cards:
    cardinfo = cards[card]
    cmd = [
        os.path.join(FLESNETDIR, "cri_cfg"),
        "-l", "2",
        "-L", f"{LOGDIR}cri{card}_cfg.log",
        "-i", f"{cardinfo['pci_address']}",
        "-t", f"{common['mc_size_ns'] // 1000}",
        "-r", f"{common['pgen_rate']}",
        "--mc-size-limit", f"{common['mc_size_limit_bytes']}",
    ]
    channels = cardinfo['channels']
    for channel in channels:
        channel_type = channels[channel]
        cmd += [f"--c{channel}_source", f"{channel_type}"]
        if channel_type == "pgen":
            cmd += [f"--c{channel}_eq_id", f"{cardinfo['pgen_base_eqid'] + channel}"]
            pgen_in_use = True
    subprocess.run(cmd)

archivable_data = not common['allow_unsupported_cri_designs']

# Start cri_server subprocesses, each in its own process group
print("Starting cri_server instance(s)...")
for card in cards:
    cardinfo = cards[card]
    cmd = [
        os.path.join(FLESNETDIR, "cri_server"),
        "-c", "/dev/null",
        "-L", f"{LOGDIR}cri{card}_server.log",
        "--log-syslog",
        "-i", f"{cardinfo['pci_address']}",
        f"--archivable-data={str(archivable_data).lower()}",
        f"--data-buffer-size-exp={common['buf_size_exp']}",
        "-o", f"{SHM_PREFIX}{card}",
        "-e", f"{os.path.join(SPMDIR, "spm-provide")} cri_server_sem",
    ]    
    process = subprocess.Popen(cmd, preexec_fn=os.setsid)
    processes.append(process)

# Block until servers are ready
print(f"Waiting for {len(cards)} cri_server instance(s)...")
subprocess.run([os.path.join(SPMDIR, "spm-require"),
                f"-n{len(cards)}", "cri_server_sem"])
print("... all cri_server(s) started")

# First opportunity to safely shutdown
if END_REQUESTED:
    end_readout()

time.sleep(1)
if pgen_in_use:
    print("Enabling pattern generators...")
    subprocess.run([FLESNETDIR + 'cri_en_pgen', '1'],
                   stdout=open(LOGDIR + 'cri_en_pgen.log', "w"))

# From this point it is safe to shut down any time
MAY_END = True
# Check for race conditions
if END_REQUESTED:
    end_readout()

subprocess.run([os.path.join(SPMDIR, "spm-provide"), "fles_input_sem"])
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
