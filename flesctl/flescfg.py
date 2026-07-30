#!/usr/bin/env python3
"""Manage flesnet and data taking configuration."""
# 2024-12-03 Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>
# pyright: reportArgumentType=false

import re
import sys

import yaml
from schema import Schema, And, Or, Use, Optional, SchemaError  # type: ignore


def parse_size(value: str | int) -> int:
    """Parse a human-readable size string into bytes."""
    if isinstance(value, int):
        return value
    suffixes = {"K": 1 << 10, "M": 1 << 20, "G": 1 << 30, "T": 1 << 40}
    val = value.strip().upper().replace(" ", "")
    if len(val) > 0 and val[-1] in suffixes:
        number = float(val[:-1])
        unit = val[-1]
    else:
        number = float(val)
        unit = ""
    return int(number * suffixes.get(unit, 1))


# Durations are passed through to the flesnet binaries verbatim. Their parser
# (Nanoseconds::parse in lib/tsb/OptionValues.cpp) requires a unit suffix and
# rejects a bare number, so validate the format here rather than converting.
DURATION_RE = re.compile(r"^-?[0-9]+(ns|us|µs|ms|s)$")


def parse_duration(value: str) -> str:
    """Validate a duration string with unit suffix, returning it unchanged."""
    val = str(value).strip()
    if not DURATION_RE.match(val):
        raise ValueError(f"invalid duration '{value}', expected <number>[ns|us|ms|s]")
    return val


# The keys of the tsmanager, stserver and tsbuilder sections are translated
# directly into options of the corresponding generated configuration file, with
# underscores replaced by dashes. Adding an option supported by one of those
# binaries requires no more than a new entry here.
CONFIG_SCHEMA = Schema(
    {
        "common": {
            # shared by tsmanager and stserver, must be identical for both
            "timeslice_duration": And(str, Use(parse_duration)),
            "tsmanager": {
                "timeout": And(str, Use(parse_duration)),
                "max_in_flight": And(Use(int), lambda n: 0 < n),
            },
            "stserver": {
                "timeout": And(str, Use(parse_duration)),
                "overlap_before": And(str, Use(parse_duration)),
                "overlap_after": And(str, Use(parse_duration)),
                # buffer sizes are per readout channel
                "data_buffer_size": And(Use(parse_size), lambda n: 0 < n),
                "desc_buffer_size": And(Use(parse_size), lambda n: 0 < n),
                # 0 disables aggregation and uses scatter-gather sends
                "aggregation_buffer_size": And(Use(parse_size), lambda n: 0 <= n),
                # software pattern generator, independent of the CRI one below
                "pgen_channels": And(Use(int), lambda n: 0 <= n),
                "pgen_microslice_duration": And(str, Use(parse_duration)),
                "pgen_microslice_size": And(Use(parse_size), lambda n: 0 < n),
                "pgen_flags": And(Use(int), lambda n: 0 <= n),
            },
            "tsbuilder": {
                "timeout": And(str, Use(parse_duration)),
                "buffer_size": And(Use(parse_size), lambda n: 0 < n),
            },
            # CRI settings, applied by cri_cfg before stserver is started
            "cri": {
                "mc_size_limit_bytes": And(Use(parse_size), lambda n: 0 < n),
                "pgen_mc_size_ns": And(Use(int), lambda n: 0 < n),
                "pgen_rate": And(Use(float), lambda x: 0 <= x <= 1),
            },
            # timeslice consumers, started on every build node
            "analyzers": [str],
            "publishers": [str],
            "recorders": [str],
            "extra_cmd": [str],
        },
        # the single node running the tsmanager the other roles connect to
        "manager_node": {
            "name": str,
            "address": str,
            Optional("ucx_net_devices"): str,
        },
        "entry_nodes": {
            Use(str): {
                "address": str,
                Optional("active"): bool,
                Optional("ucx_net_devices"): str,
                "cards": {
                    Use(str): {
                        "pci_address": str,
                        "pgen_base_eqid": Use(int),
                        "channels": {
                            int: {
                                "mode": And(
                                    str, Use(str.lower), Or("flim", "pgen", "disable")
                                ),
                            },
                        },
                    },
                },
            }
        },
        "build_nodes": {
            Use(str): {
                "address": str,
                Optional("active"): bool,
                Optional("ucx_net_devices"): str,
                Optional("analyzers"): [str],
                Optional("publishers"): [str],
                Optional("recorders"): [str],
                Optional("extra_cmd"): [str],
            }
        },
    }
)


# Defaults match the compiled-in defaults of the corresponding binaries.
CONFIG_DEFAULTS = {
    "common": {
        "timeslice_duration": "40ms",
        "tsmanager": {
            "timeout": "1s",
            "max_in_flight": 8,
        },
        "stserver": {
            "timeout": "100ms",
            "overlap_before": "100us",
            "overlap_after": "100us",
            "data_buffer_size": "1G",
            "desc_buffer_size": "16M",
            "aggregation_buffer_size": "10G",
            "pgen_channels": 0,
            "pgen_microslice_duration": "400us",
            "pgen_microslice_size": "100K",
            "pgen_flags": 3,
        },
        "tsbuilder": {
            "timeout": "10s",
            "buffer_size": "20G",
        },
        "cri": {
            "mc_size_limit_bytes": 2097152,
            "pgen_rate": 1,
        },
        "analyzers": [],
        "publishers": [],
        "recorders": [],
        "extra_cmd": [],
    }
}


def recursive_merge(dict1: dict, dict2: dict) -> dict:
    """Recursively merge two dictionaries."""
    for key, value in dict2.items():
        if key in dict1 and isinstance(dict1[key], dict) and isinstance(value, dict):
            # Recursively merge nested dictionaries
            dict1[key] = recursive_merge(dict1[key], value)
        else:
            # Merge non-dictionary values
            dict1[key] = value
    return dict1


def load_yaml(file_paths: str | list[str], default: dict | None = None) -> dict:
    """Load and merge YAML configuration files."""
    if default is None:
        default = {}
    if isinstance(file_paths, str):
        file_paths = [file_paths]

    data = default.copy()
    for file_path in file_paths:
        try:
            with open(file_path, "r", encoding="utf8") as file:
                new_data = yaml.safe_load(file)
                data = recursive_merge(data, new_data)
        except FileNotFoundError:
            print(f"File {file_path} not found.")
        except yaml.YAMLError as exc:
            print(f"Error parsing YAML file {file_path}: {exc}")

    return data


def load(file_paths: str | list[str]) -> dict | None:
    """Load and validate configuration files."""
    data = load_yaml(file_paths, CONFIG_DEFAULTS)
    try:
        validated = CONFIG_SCHEMA.validate(data)
        return validated
    except SchemaError as e:
        print(e)
        return None


def dump(data: dict) -> str:
    """Dump data to a YAML string."""
    return yaml.dump(data, default_flow_style=False, sort_keys=False)


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="Manage flesnet and data taking configuration"
    )
    parser.add_argument("config_fn", nargs="+", help="configuration file(s)")
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="print combined configuration"
    )
    args = parser.parse_args()

    config = load(args.config_fn)
    if config is not None:
        print("Configuration is valid.")
        if args.verbose:
            print(config)
        sys.exit(0)
    else:
        print("Configuration is invalid.")
        sys.exit(1)
