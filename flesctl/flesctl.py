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
import flescfg
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


class GlobalConfig:
    """Global configuration class."""

    def __init__(self) -> None:
        """Initialize global configuration."""
        self.config = configparser.ConfigParser()
        self.config.read(FLESCTL_CONF)
        # NextRunID is a mandatory integer configuration parameter
        next_run_id_: int | None = self.config["DEFAULT"].getint("NextRunID")
        if next_run_id_ is None:
            raise ValueError("NextRunID is None")
        self.next_run_id: int = next_run_id_

    def get_next_run_id(self) -> int | None:
        """Return the next run id."""
        return self.next_run_id

    def increment_next_run_id(self) -> None:
        """Increment the next run id."""
        self.next_run_id += 1
        self.config["DEFAULT"]["NextRunID"] = str(self.next_run_id)
        with open(FLESCTL_CONF, "w", encoding="utf8") as cfile:
            self.config.write(cfile)

    def get_previous_run_id(self) -> int | None:
        """Return the previous run id."""
        if self.next_run_id is None:
            return None
        return self.next_run_id - 1

    def get_reservation(self) -> str | None:
        """Return the reservation name."""
        return self.config["DEFAULT"].get("Reservation", None)


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


def all_tags():
    """Yield all available configuration tags."""
    for filename in glob.iglob(confdir + "/**/*.yaml", recursive=True):
        if not filename.startswith(confdir + "/"):
            raise ValueError("Filename does not start with confdir")
        tag, _ = os.path.splitext(filename[len(confdir + "/") :])
        yield tag


def list_tags() -> None:
    """List all available configuration tags."""
    for tag in sorted(all_tags()):
        print(tag)


def validate_tags(tags: str) -> None:
    """Check if tags exist."""
    tag_list: list[str] = tags.split(",")
    unknown_tags = [tag for tag in tag_list if tag not in all_tags()]
    if unknown_tags:
        print(f"error: tag unknown: {','.join(unknown_tags)}")
        sys.exit(1)


def get_tag_config(tags: str) -> dict | None:
    """Get the configuration dictionary of a tag list."""
    validate_tags(tags)
    tag_list: list[str] = tags.split(",")
    config_files = [os.path.join(confdir, tag + ".yaml") for tag in tag_list]
    return flescfg.load(config_files)


def get_tag_config_str(tags: str) -> str:
    """Get the configuration of a tag list as string."""
    cfg = get_tag_config(tags)
    if cfg is None:
        print("error: invalid configuration")
        sys.exit(1)
    out: str = ""
    for tag in tags.split(","):
        out += f"# {tag}\n"
    out += flescfg.dump(cfg)
    return out


def print_or_pager(out: str) -> None:
    """Print the output or pipe it to the pager specified by the PAGER environment variable."""
    if sys.stdout.isatty():
        pager = os.environ.get("PAGER", "/usr/bin/less")
        pager_bin = shutil.which(pager)
        if pager_bin is None:
            print(out, end="")
            return
        subprocess.run([pager_bin], input=out, text=True, check=False)
    else:
        print(out, end="")


def print_config(tags: str) -> None:
    """Print the configuration of a tag list."""
    out: str = get_tag_config_str(tags)
    print_or_pager(out)


def get_tags(
    cfg: GlobalConfig, tags: str | None = None, run_id: int | None = None
) -> str:
    """Optionally get the tags from a previous run."""
    if tags is not None:
        return tags
    if run_id is None:
        run_id = cfg.get_previous_run_id()
    if run_id is None:
        print("error: no tags specified")
        sys.exit(1)
    from_runconf = configparser.ConfigParser()
    from_runconf.read(os.path.join(RUNDIR_BASE, str(run_id), "run.conf"))
    tags = from_runconf["DEFAULT"]["tag"]
    return tags


def start(
    cfg: GlobalConfig,
    tags: str,
    simulate: bool = False,
) -> None:
    """Start a run with given tags."""
    # check if readout is active
    output = subprocess.check_output(
        ["/usr/bin/squeue", "-h", "-u", RUN_AS_USER], universal_newlines=True
    )
    if len(output) > 0:
        print("error: job is already active")
        print(output, end="")
        sys.exit(1)

    # get joint tag configuration (also checks for validity)
    joint_config = get_tag_config_str(tags)

    # retrieve next run id and reservation
    next_run_id: int | None = cfg.get_next_run_id()
    if next_run_id is None:
        print("error: no valid configuration at", FLESCTL_CONF)
        sys.exit(1)
    else:
        run_id: int = next_run_id
    reservation = cfg.get_reservation()

    if simulate:
        print("Simulation mode:")
        print(f"Tags: {tags}")
        print(f"Run ID: {run_id}")
        print(f"Reservation: {reservation}")
        return

    # create run directory
    rundir = os.path.join(RUNDIR_BASE, str(run_id))
    try:
        os.mkdir(rundir)
    except FileExistsError:
        print("error: run directory", rundir, "exists")
        sys.exit(1)
    os.chdir(rundir)
    os.mkdir("log")

    # increment stored next run id
    cfg.increment_next_run_id()
    print(f"starting run with id {run_id}")

    # TODO: check prerequisites, e.g. leftovers from previous runs  # pylint: disable=fixme

    # create run-local version of the joint tag config
    with open("readout.yaml", "w", encoding="utf8") as config_file:
        config_file.write(joint_config)

    # create run configuration file
    start_time = time.time()
    start_user = os.environ["SUDO_USER"]
    tag_str = ",".join(tags)
    runconf = configparser.ConfigParser()
    runconf["DEFAULT"] = {
        "Tag": tag_str,
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
    log_msg += f" Tag: {tag_str}"
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
    mattermost_msg = f":white_check_mark:  Run {run_id} started with tag '{tag_str}'"
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


def run_info(cfg: GlobalConfig, par_run_id: int | None = None) -> None:
    """Print information on the latest run or a specified run."""
    if par_run_id is None:
        info_run_id = cfg.get_previous_run_id()
    else:
        info_run_id = par_run_id

    config_file = os.path.join(RUNDIR_BASE, str(info_run_id), "run.conf")
    if not os.path.isfile(config_file):
        print("error: no such run found")
        sys.exit(1)

    runconf = configparser.ConfigParser()
    runconf.read(config_file)
    tag: str | None = runconf["DEFAULT"].get("tag", None)
    nodelist: str | None = None
    starttime: int | None = runconf["DEFAULT"].getint("starttime", None)
    stoptime: int | None = runconf["DEFAULT"].getint("stoptime", None)
    startuser: str = runconf["DEFAULT"].get("startuser", "N/A")
    stopuser: str = runconf["DEFAULT"].get("stopuser", "N/A")
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
    starttime_str: str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(starttime))
    stoptime_str: str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(stoptime))
    start_str: str = starttime_str + " by " + startuser
    stop_str: str = stoptime_str + " by " + stopuser

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


def main():
    """Main function."""
    check_user_or_exit()
    # flesctl main parser
    parser = argparse.ArgumentParser(
        description="Control configuration and data taking on mFLES"
    )
    parser.add_argument("--version", action="version", version="0.3")
    subparsers = parser.add_subparsers(
        dest="command", required=True, help="flesctl command"
    )
    # flesctl list
    subparsers.add_parser(
        "list",
        help="List available configuration tags",
        description="List all available configuration tags",
    )
    # flesctl show
    parser_show = subparsers.add_parser(
        "show",
        help="Show the contents of a given configuration",
        description="Print the contents of a list of configuration tags"
        "Use --from-run to use the configuration tags from a previous run.",
    )
    group_show = parser_show.add_mutually_exclusive_group(required=True)
    group_show.add_argument(
        "-r",
        "--from-run",
        type=int,
        help="run id to use configuration tags from a previous run",
    )
    group_show.add_argument(
        "-t",
        "--tag",
        help="configuration tag list (comma separated)",
    )
    # flesctl start
    parser_start = subparsers.add_parser(
        "start",
        help="Start a run",
        description="Start a run with one or more given configuration tags. "
        "Use --from-run to use the configuration tags from a previous run.",
    )
    parser_start.add_argument(
        "-s",
        "--simulate",
        action="store_true",
        default=False,
        help="no action; simulate the run start without executing it",
    )
    group = parser_start.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "-r",
        "--from-run",
        type=int,
        help="run id to use configuration tags from a previous run",
    )
    group.add_argument(
        "-t",
        "--tag",
        help="configuration tag list (comma separated)",
    )
    # flesctl stop
    subparsers.add_parser(
        "stop",
        help="Stop the ongoing run",
        description="Stop a run by cancelling the slurm job",
    )
    # flesctl monitor
    subparsers.add_parser(
        "monitor",
        aliases=["mon"],
        help="open the run monitor",
        description="Open the run monitor. This will show the main log of the current run.",
    )
    # flesctl status
    parser_status = subparsers.add_parser(
        "status",
        aliases=["info"],
        help="Print information on latest run or a specified run",
        description="Print information on the latest run or a specified run",
    )
    parser_status.add_argument("run", nargs="?", type=int, help="run number (optional)")

    args = parser.parse_args()

    # read global configuration
    config = GlobalConfig()

    if args.command == "list":
        list_tags()
    elif args.command == "show":
        tags = get_tags(cfg=config, tags=args.tag, run_id=args.from_run)
        print_config(tags=tags)
    elif args.command == "start":
        tags = get_tags(cfg=config, tags=args.tag, run_id=args.from_run)
        start(cfg=config, tags=tags, simulate=args.simulate)
    elif args.command == "stop":
        stop()
    elif args.command in ("monitor", "mon"):
        monitor()
    elif args.command in ("status", "info"):
        run_info(cfg=config, par_run_id=args.run)


if __name__ == "__main__":
    main()
