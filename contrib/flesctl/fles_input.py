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
    
# Start processes
SERVER_PID = []

def cleanup():
    print("Cleaning up ...")
    for pid in SERVER_PID:
        os.killpg(os.getpgid(pid), signal.SIGKILL)
    for shm_file in glob.glob('/dev/shm/{}*'.format(SHM_PREFIX)):
        os.remove(shm_file)
    sys.exit(-1)

def end_readout():
    print("Shutting down ...")
    subprocess.run([FLESNETDIR + 'cri_en_pgen', '0'], stdout=open(LOGDIR + 'cri_en_pgen.log', 'a'))
    for pid in SERVER_PID:
        os.kill(pid, signal.SIGINT)
    time.sleep(10)
    for pid in SERVER_PID:
        try:
            os.kill(pid, 0)
        except OSError:
            continue
        print("Cleanup required, cri_server left.")
        cleanup()
    print("Exiting")
    sys.exit(0)

END_REQUESTED = False

def request_end(signum, frame):
    global END_REQUESTED
    END_REQUESTED = True

# set end request on signal
signal.signal(signal.SIGINT, request_end)
signal.signal(signal.SIGTERM, request_end)
signal.signal(signal.SIGHUP, request_end)

print("Starting readout ...")
subprocess.run([FLESNETDIR + 'cri_info'], stdout=open(LOGDIR + 'cri_info.log', 'a'))

print("Configuring CRIs ...")
# round up to next possible size (given mc_size is min. size)
#PGEN_MC_SIZE=`echo "scale=0; (${MC_SIZE_NS}+1024-1)/1024" | bc -l`
# round down to next possible size (given mc_size is max. size)
MC_SIZE_NS = config['common']['mc_size_ns']
PGEN_MC_SIZE = MC_SIZE_NS // 1000
PGEN_RATE = config['common']['pgen_rate']
MC_SIZE_LIMIT_BYTES = config['common']['mc_size_limit_bytes']
cards = config['entry_nodes'][hostname]['cards']
for card in cards:
    cardinfo = cards[card]
    cmd = [
        f"{FLESNETDIR}cri_cfg",
        "-l", "2",
        "-L", f"{LOGDIR}cri{card}_cfg.log",
        "-i", f"{cardinfo['pci_address']}",
        "-t", f"{PGEN_MC_SIZE}",
        "-r", f"{PGEN_RATE}",
        "--mc-size-limit", f"{MC_SIZE_LIMIT_BYTES}",
    ]
    channels = cardinfo['channels']
    for channel in channels:
        channel_type = channels[channel]
        cmd.append([
            f"--c{channel}_source", f"{channel_type}",
            f"--c{channel}_eq_id", f"{card['pgen_base_eqid'] + channel}",
        ])
    subprocess.run(cmd)

ALLOW_UNSUPPORTED_CRI_DESIGNS = config['common']['allow_unsupported_cri_designs']
ARCH_DATA = not ALLOW_UNSUPPORTED_CRI_DESIGNS

print("Starting cri_server ...")
for card in cards:
    cardinfo = cards[card]
    cmd = [
        f"{FLESNETDIR}cri_server",
        "-c", "/dev/null",
        "-L", f"{LOGDIR}cri{card}_server.log",
        "--log-syslog",
        "-i", f"{cardinfo['pci_address']}",
        f"--archivable-data={str(ARCH_DATA).lower()}",
        f"--data-buffer-size-exp", "{BUF_SIZE_EXP}",
        "-o", f"{SHM_PREFIX}{card}",
        "-e", f"\"{SPMDIR}spm-provide cri_server_sem\"",
    ]
    process = subprocess.Popen(cmd, preexec_fn=os.setsid)
    SERVER_PID.append(process.pid)

# Get group ID for cleanup
SID = subprocess.check_output(["ps", "o", "sid", "-p", str(SERVER_PID[0])]).decode().splitlines()[-1]

# Block till server is ready
print(f"Waiting for {len(SERVER_PID)} cri_server instance(s) ...")
subprocess.run([f"{SPMDIR}spm-require", f"-n{len(SERVER_PID)}", "cri_server_sem"])
print("... all cri_server(s) started")

# First opportunity to safely shutdown
if END_REQUESTED:
    end_readout()

time.sleep(1)
print("Enabling pattern generators ...")
subprocess.run([FLESNETDIR + 'cri_en_pgen', '1'], stdout=open(LOGDIR + 'cri_en_pgen.log', 'a'))

# From this point it is safe to shut down any time
signal.signal(signal.SIGINT, end_readout)
signal.signal(signal.SIGTERM, end_readout)
signal.signal(signal.SIGHUP, end_readout)

# Check for race conditions
if END_REQUESTED:
    end_readout()

subprocess.run([f"{SPMDIR}spm-provide", "fles_input_sem"])
print("Running ...")
try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    end_readout()

print("*** YOU SHOULD NEVER SEE THIS ***")
