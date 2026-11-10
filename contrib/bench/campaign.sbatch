#!/bin/bash
# Run one flesbench scenario in an exclusive allocation.
#   sbatch -N 4 campaign.sbatch scenarios/dev-combined.yaml [--out DIR]
#SBATCH --job-name=flesbench
#SBATCH --exclusive
#SBATCH --mem=0
#SBATCH --time=01:00:00
#SBATCH --output=flesbench-%j.out
#SBATCH --signal=B:TERM@120

set -euo pipefail
cd "${SLURM_SUBMIT_DIR:-.}"
# sbatch runs a spooled copy of this script, so find the original location
script=$(scontrol show job "$SLURM_JOB_ID" | sed -n 's/^ *Command=\([^ ]*\).*/\1/p')
# exec so that the TERM sent before the time limit reaches flesbench.py
exec python3 "$(dirname "$(readlink -f "$script")")/flesbench.py" "$@"
