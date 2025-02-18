#!/usr/bin/env python3
"""Control configuration and data taking on mFLES."""
# 2018-11-06 Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>
# 2018-11-06 Dirk Hutter <hutter@compeng.uni-frankfurt.de>
# 2025-02-07 Jan de Cuveland

import argparse
import configparser
import datetime
import glob
import os
import pwd
import shutil
import signal
import subprocess
import sys
import time

import elog  # type: ignore
import init_run
import requests
from rich import print as rprint
from rich.panel import Panel
from rich.table import Column, Table

scriptdir = os.path.dirname(os.path.realpath(__file__))
confdir = os.path.normpath("/home/flesctl/config")
RUNDIR_BASE = "/home/flesctl/run"
FLESCTL_CONF = "/home/flesctl/private/flesctl.conf"
FLESCTL_SYSLOG = "/var/log/remote_serv/flesctl.log"
RUN_AS_USER = "flesctl"

# elog configuration
ELOG_HOST = "http://mcbmgw01:8080/mCBM/"
ELOG_ATTR_STATIC = {"author": "flesctl", "type": "Routine", "category": "mFLES"}

# mattermost configuration
MATTERMOST_HOST = "https://mattermost.web.cern.ch/hooks/8i895g6rmjbqdjop3m6ee3t4qo"
MATTERMOST_ATTR_STATIC = {"channel": "mfles-status", "username": "flesctl"}


def get_user_fullname(user: str) -> str:
    """Get the full name of a user."""
    try:
        fullname = pwd.getpwnam(user)[4].split(",")[0]
        if not fullname:
            fullname = user
    except KeyError:
        fullname = user
    return fullname


def check_user_or_exit() -> None:
    """Check if run as correct user."""
    username = pwd.getpwuid(os.getuid()).pw_name
    sudo_user = os.environ.get("SUDO_USER")
    if sudo_user is None or username != RUN_AS_USER:
        print("start using sudo as user", RUN_AS_USER)
        sys.exit()


# read global configuration
config = configparser.ConfigParser()
config.read(FLESCTL_CONF)
run_id = config["DEFAULT"].getint("NextRunID")
if not run_id:
    print("error: no configuration at", FLESCTL_CONF)
    sys.exit(1)
reservation = config["DEFAULT"].get("Reservation", None)


def tags():
    """Yield all available configuration tags."""
    for filename in glob.iglob(confdir + "/**/*.yaml", recursive=True):
        if not filename.startswith(confdir + "/"):
            raise ValueError("Filename does not start with confdir")
        tag, _ = os.path.splitext(filename[len(confdir + "/") :])
        yield tag


def list_tags() -> None:
    """List all available configuration tags."""
    for tag in sorted(tags()):
        print(tag)


def print_config(tag: str) -> None:
    """Print the configuration of a tag."""
    # check if tag exists
    if tag not in tags():
        print("error: tag unknown")
        sys.exit(1)
    print("# Tag:", tag)
    with open(os.path.join(confdir, tag + ".yaml"), "r", encoding="utf8") as cfile:
        if sys.stdout.isatty():
            subprocess.run(
                ["/usr/bin/less"], input=cfile.read(), text=True, check=False
            )
        else:
            print(cfile.read())


def start(tag: str) -> None:
    """Start a run with a given tag."""
    # check if readout is active
    output = subprocess.check_output(
        ["/usr/bin/squeue", "-h", "-u", RUN_AS_USER], universal_newlines=True
    )
    if len(output) > 0:
        print("error: job is already active")
        print(output, end="")
        sys.exit(1)

    # check if tag exists
    if tag not in tags():
        print("error: tag unknown")
        sys.exit(1)

    # create run directory
    rundir = os.path.join(RUNDIR_BASE, str(run_id))
    try:
        os.mkdir(rundir)
    except FileExistsError:
        print("error: run directory", rundir, "exists")
        sys.exit(1)
    os.chdir(rundir)
    os.mkdir("log")

    # increment next run id
    config["DEFAULT"]["NextRunID"] = str(run_id + 1)
    with open(FLESCTL_CONF, "w", encoding="utf8") as configfile:
        config.write(configfile)
    print("starting run with id", run_id)

    # TODO: check prerequisites, e.g. leftovers from previous runs

    # create run-local copy of tag config
    shutil.copy(os.path.join(confdir, tag + ".yaml"), "readout.yaml")

    # create run configuration file
    start_time = time.time()
    start_user = os.environ["SUDO_USER"]
    runconf = configparser.ConfigParser()
    runconf["DEFAULT"] = {
        "Tag": tag,
        "RunId": str(run_id),
        "StartTime": str(int(start_time)),
        "StartUser": start_user,
    }
    with open("run.conf", "w", encoding="utf8") as runconffile:
        runconf.write(runconffile)

    # create spm and flesnet configuration from tag
    init_run.main("readout.yaml", str(run_id))

    # start run using spm
    cmd = [
        "/opt/spm/spm-run",
        "--batch",
        "--logfile",
        "slurm.out",
        "--jobname",
        f"run_{run_id}",
        "--time",
        "0",
    ]
    if reservation:
        cmd += ["--reservation", reservation]
    subprocess.call(cmd + ["readout.spm"])

    # create elog entry
    now = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(start_time))
    log_msg = f"Run started at {now}\n"
    log_msg += f" Tag: {tag}"
    try:
        logbook = elog.open(ELOG_HOST)
        log_id_start = logbook.post(
            log_msg, attributes=ELOG_ATTR_STATIC, RunNumber=run_id, subject="Run start"
        )
    except elog.LogbookError:
        print("Error: electronic logbook not reachable")
        log_id_start = -1

    runconf["DEFAULT"]["elog_id_start"] = str(log_id_start)
    with open("run.conf", "w", encoding="utf8") as runconffile:
        runconf.write(runconffile)

    # create mattermost message
    mattermost_msg = f":white_check_mark:  Run {run_id} started with tag '{tag}'"
    mattermost_data = MATTERMOST_ATTR_STATIC.copy()
    mattermost_data["text"] = mattermost_msg
    try:
        requests.post(url=MATTERMOST_HOST, json=mattermost_data, timeout=10)
    except requests.exceptions.RequestException as e:
        print(f"Error: Mattermost exception: {e}")


def current_run_id() -> int | None:
    """Return the current run id or None if no run is active."""
    output = subprocess.check_output(
        ["/usr/bin/squeue", "-h", "-o", "%j", "-u", RUN_AS_USER],
        universal_newlines=True,
    )
    if output.startswith("run_"):
        return int(output[len("run_") :])
    return None


def current_nodelist() -> str | None:
    """Return the current nodelist or None if no run is active."""
    output = subprocess.check_output(
        ["/usr/bin/squeue", "-h", "-o", "%R", "-u", RUN_AS_USER],
        universal_newlines=True,
    )
    if len(output) > 0:
        return output.rstrip()
    return None


def stop() -> None:
    """Stop the ongoing run."""
    run_id = current_run_id()
    if run_id is None:
        print("error: no run job active")
        sys.exit(1)

    # change to run directory
    rundir = os.path.join(RUNDIR_BASE, str(run_id))
    os.chdir(rundir)

    print("stopping run with id", run_id)
    subprocess.call(["/usr/bin/scancel", "--jobname", f"run_{run_id}"])
    stop_time = time.time()
    stop_user = os.environ["SUDO_USER"]

    # read run configuration file
    runconf = configparser.ConfigParser()
    runconf.read("run.conf")
    start_time = int(runconf["DEFAULT"]["StartTime"])
    log_id_start = int(runconf["DEFAULT"]["elog_id_start"])

    # create elog entry
    now = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(stop_time))
    dt_s: int = int(stop_time) - start_time
    duration = str(datetime.timedelta(seconds=dt_s))
    log_msg = f"Run stopped at {now}\n"
    log_msg += f" Duration: {duration}"
    try:
        logbook = elog.open(ELOG_HOST)
        # do not reply if elog failed during start
        if log_id_start == -1:
            log_id_stop = logbook.post(
                log_msg,
                attributes=ELOG_ATTR_STATIC,
                RunNumber=run_id,
                subject="Run stop",
            )
        else:
            log_id_stop = logbook.post(
                log_msg,
                msg_id=log_id_start,
                reply=True,
                attributes=ELOG_ATTR_STATIC,
                RunNumber=run_id,
                subject="Run stop",
            )
    except elog.LogbookError:
        print("Error: electronic logbook not reachable")
        log_id_stop = -1

    # update run configuration file
    runconf["DEFAULT"]["StopTime"] = str(int(stop_time))
    runconf["DEFAULT"]["StopUser"] = stop_user
    runconf["DEFAULT"]["elog_id_stop"] = str(log_id_stop)
    with open("run.conf", "w", encoding="utf8") as runconffile:
        runconf.write(runconffile)

    # create mattermost message
    dt_s = int(stop_time) - start_time
    duration = str(datetime.timedelta(seconds=dt_s))
    mattermost_msg = f":stop_sign:  Run {run_id} stopped after {duration}"
    mattermost_data = MATTERMOST_ATTR_STATIC.copy()
    mattermost_data["text"] = mattermost_msg
    try:
        requests.post(url=MATTERMOST_HOST, json=mattermost_data, timeout=10)
    except requests.exceptions.RequestException as e:
        print(f"Error: Mattermost exception: {e}")

    # Cleanup: remove leftovers (currently not implemented)


def monitor() -> None:
    """Open the run monitor."""
    syslog_mon = True
    if syslog_mon:
        # syslog based monitoring
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        subprocess.call(["/usr/bin/less", "-n", "+G", "+F", FLESCTL_SYSLOG])
    else:
        # slurm log based monitoring
        run_id = current_run_id()
        if run_id is None:
            print("error: no active run found")
            sys.exit(1)
        subprocess.call(
            ["/usr/bin/tail", "-f", f"/home/flesctl/run/{run_id}/slurm.out"]
        )


def run_info(par_run_id=None) -> None:
    """Print information on the latest run or a specified run."""
    if par_run_id is None:
        info_run_id = run_id - 1
    else:
        info_run_id = par_run_id

    config_file = os.path.join(RUNDIR_BASE, str(info_run_id), "run.conf")
    if not os.path.isfile(config_file):
        print("error: no such run found")
        sys.exit(1)

    runconf = configparser.ConfigParser()
    runconf.read(config_file)
    tag = runconf["DEFAULT"].get("tag", None)
    nodelist = None
    starttime = runconf["DEFAULT"].getint("starttime", None)
    stoptime = runconf["DEFAULT"].getint("stoptime", None)
    startuser = runconf["DEFAULT"].get("startuser", "N/A")
    stopuser = runconf["DEFAULT"].get("stopuser", "N/A")
    duration = None

    # this order of checks is needed to prevent a race condition
    # when a run is stopped and ends during this call
    if current_run_id() == info_run_id:
        nodelist = current_nodelist()
        if stoptime is not None:
            status = "pending finalization"
            if starttime is not None:
                dt_s = stoptime - starttime
                duration = datetime.timedelta(seconds=dt_s)
        else:
            status = "running"
            if starttime is not None:
                dt_s = int(time.time()) - starttime
                duration = datetime.timedelta(seconds=dt_s)
    else:
        if stoptime is not None:
            status = "stopped"
            if starttime is not None:
                dt_s = stoptime - starttime
                duration = datetime.timedelta(seconds=dt_s)
        else:
            status = "ended unexpectedly"

    # retrieve additional information from run config
    components = None
    flesnet_conf = os.path.join(RUNDIR_BASE, str(info_run_id), "flesnet.cfg")
    if os.path.isfile(flesnet_conf):
        output = subprocess.check_output(
            ["grep", "-c", "^input =", flesnet_conf], universal_newlines=True
        )
        components = output.rstrip()

    # assemble output
    startuser = get_user_fullname(startuser)
    stopuser = get_user_fullname(stopuser)
    starttime_str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(starttime))
    stoptime_str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(stoptime))
    start_str = starttime_str + " by " + startuser
    stop_str = stoptime_str + " by " + stopuser

    t = Table(
        Column(justify="right"),
        Column(),
        box=None,
        show_header=False,
        padding=(0, 1, 0, 0),
    )
    t.add_row("Run number:", str(info_run_id))
    if par_run_id is not None:
        t.add_row("Status:", status)
    else:
        highlite_status = {
            "pending finalization": "[bold red]PENDING FINALIZATION[/]",
            "running": "[bold green]RUNNING[/]",
            "stopped": "[bold blue]STOPPED[/]",
            "ended unexpectedly": "[bold red]ENDED UNEXPECTEDLY[/]",
        }
        t.add_row("Status:", highlite_status[status])
        if nodelist is not None:
            t.add_row("Slurm nodelist:", nodelist)
    t.add_row("Configuration:", tag)
    if components is not None:
        t.add_row("Components:", components)
    t.add_row("Started:", start_str)
    if stoptime is not None:
        t.add_row("Stopped:", stop_str)
    if duration is not None:
        t.add_row("Duration:", str(duration))
    rprint(Panel.fit(t, title="Run " + str(info_run_id)))


if __name__ == "__main__":
    check_user_or_exit()

    parser = argparse.ArgumentParser(
        description="Control configuration and data taking on mFLES"
    )
    parser.add_argument("--version", action="version", version="0.3")
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("list", help="list available configuration tags")

    parser_show = subparsers.add_parser("show", help="print configuration of <tag>")
    parser_show.add_argument("tag", help="configuration tag")

    parser_start = subparsers.add_parser(
        "start", help="start a run with configuration <tag>"
    )
    parser_start.add_argument("tag", help="configuration tag")

    subparsers.add_parser("stop", help="stop the ongoing run")

    subparsers.add_parser("monitor", aliases=["mon"], help="open the run monitor")

    parser_status = subparsers.add_parser(
        "status",
        aliases=["info"],
        help="print information on latest run or a specified run",
    )
    parser_status.add_argument("run", nargs="?", type=int, help="run number (optional)")

    args = parser.parse_args()

    if args.command == "list":
        list_tags()
    elif args.command == "show":
        print_config(args.tag)
    elif args.command == "start":
        start(args.tag)
    elif args.command == "stop":
        stop()
    elif args.command in ("monitor", "mon"):
        monitor()
    elif args.command in ("status", "info"):
        run_info(args.run)
