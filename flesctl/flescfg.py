#!/usr/bin/env python3
"""Manage flesnet and data taking configuration."""
# 2024-12-03 Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>
# pyright: reportArgumentType=false

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


CONFIG_SCHEMA = Schema(
    {
        "use_entry_nodes": str,
        "use_build_nodes": str,
        "common": {
            "timeslice_size": And(Use(int), lambda n: 0 < n),
            "timeslice_overlap": And(Use(int), lambda n: 0 < n < 10000),
            "tsbuf_data_size_exp": And(Use(int), lambda n: 0 < n < 10000),
            "tsbuf_desc_size_exp": And(Use(int), lambda n: 0 < n < 10000),
            "transport": And(str, Use(str.lower), Or("rdma", "libfabric", "zeromq")),
            "mc_size_limit_bytes": And(Use(parse_size), lambda n: 0 < n),
            "default_readout_buffer_size": And(Use(parse_size), lambda n: 0 < n),
            "pgen_mc_size_ns": And(Use(int), lambda n: 0 < n),
            "pgen_rate": And(Use(float), lambda x: 0 <= x <= 1),
            "tsclient_param": [str],
            "extra_cmd": [str],
        },
        "entry_nodes": {
            Use(str): {
                "address": str,
                "cards": {
                    Use(str): {
                        "pci_address": str,
                        "pgen_base_eqid": Use(int),
                        "channels": {
                            int: {
                                "mode": And(
                                    str, Use(str.lower), Or("flim", "pgen", "disable")
                                ),
                                Optional("readout_buffer_size"): And(
                                    Use(parse_size), lambda n: 0 < n
                                ),
                            },
                        },
                        Optional("default_readout_buffer_size"): And(
                            Use(parse_size), lambda n: 0 < n
                        ),
                    },
                },
            }
        },
        "build_nodes": {
            Use(str): {
                "address": str,
                Optional("tsclient_param"): [str],
                Optional("extra_cmd"): [str],
            }
        },
    }
)


CONFIG_DEFAULTS = {
    "common": {
        "timeslice_size": 1000,
        "timeslice_overlap": 1,
        "transport": "rdma",
        "mc_size_limit_bytes": 2097152,
        "default_readout_buffer_size": 2 ^ 31,
        "pgen_rate": 1,
        "tsclient_param": [],
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
