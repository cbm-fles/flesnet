#!/usr/bin/env python3

# Manage flesnet and data taking configuration
#
# 2024-12-03 Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>

import yaml
from schema import Schema, And, Or, Use, Optional, SchemaError

CONFIG_SCHEMA = Schema({
  "use_entry_nodes": str,
  "use_build_nodes": str,
  "common": {
    "timeslice_size": And(Use(int), lambda n: 0 < n),
    "timeslice_overlap": And(Use(int), lambda n: 0 < n < 10000),
    "tsbuf_data_size_exp": And(Use(int), lambda n: 0 < n < 10000),
    "tsbuf_desc_size_exp": And(Use(int), lambda n: 0 < n < 10000),
    "transport": And(str, Use(str.lower), Or("rdma", "libfabric", "zeromq")),
    "allow_unsupported_cri_designs": bool,
    "mc_size_limit_bytes": And(Use(int), lambda n: 0 < n),
    "buf_size_exp": And(Use(int), lambda n: 0 < n < 10000),
    "mc_size_ns": And(Use(int), lambda n: 0 < n),
    "pgen_rate": And(Use(int), lambda n: 0 < n < 10000),
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
            int: And(str, Use(str.lower), Or("flim", "pgen", "disable")),
          }
        }
      }
    }
  },
  "build_nodes": {
    Use(str): {
      "address": str,
      Optional("tsclient_param"): [str],
      Optional("extra_cmd"): [str],
    }
  }
})


CONFIG_DEFAULTS = {
  "common": {
    "timeslice_size": 1000,
    "timeslice_overlap": 1,
    "transport": "rdma",
    "allow_unsupported_cri_designs": False,
    "mc_size_limit_bytes": 2097152,
    "buf_size_exp": 31,
    "mc_size_ns": 100000,
    "pgen_rate": 1,
    "tsclient_param": [],
    "extra_cmd": [],
  }
}


def recursive_merge(dict1, dict2):
  for key, value in dict2.items():
    if key in dict1 and isinstance(dict1[key], dict) and isinstance(value, dict):
      # Recursively merge nested dictionaries
      dict1[key] = recursive_merge(dict1[key], value)
    else:
      # Merge non-dictionary values
      dict1[key] = value
  return dict1


def load_yaml(file_paths: str | list[str], default: dict={}):
  if isinstance(file_paths, str):
    file_paths = [file_paths]

  data = default.copy()
  for file_path in file_paths:
    try:
      with open(file_path, 'r') as file:
        new_data = yaml.safe_load(file)
        data = recursive_merge(data, new_data)
    except FileNotFoundError:
      print(f"File {file_path} not found.")
    except yaml.YAMLError as exc:
      print(f"Error parsing YAML file {file_path}: {exc}")

  return data


def validate_yaml(yaml_data):
  try:
    CONFIG_SCHEMA.validate(yaml_data)
    return True
  except SchemaError as e:
    print(e)
    return False


def load(file_paths: str | list[str]):
  data = load_yaml(file_paths, CONFIG_DEFAULTS)
  if validate_yaml(data):
    return data
  else:
    return None
  

if __name__ == "__main__":
  import argparse

  parser = argparse.ArgumentParser(description="Manage flesnet and data taking configuration")
  parser.add_argument("config_fn", nargs='+', help="configuration file(s)")
  parser.add_argument("-v", "--verbose", action="store_true", help="print combined configuration")
  args = parser.parse_args()

  config = load(args.config_fn)
  if config is not None:
    print("Configuration is valid.")
    if args.verbose:
      print(config)
    exit(0)
  else:
    print("Configuration is invalid.")
    exit(1)

