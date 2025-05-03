"""Utilities for interacting with Slurm."""

import subprocess


def node_list(nodes: str) -> list[str]:
    """Expand a Slurm node specification list into individual node names."""
    nodelist = (
        subprocess.check_output(
            ["scontrol", "show", "hostname", nodes]
        )
        .decode()
        .split()
    )
    return nodelist
