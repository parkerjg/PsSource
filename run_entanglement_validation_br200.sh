#!/usr/bin/env bash
#
# Entanglement-modulation validation of PsSource 2-gamma delegation on
# IU Big Red 200.
#
# Run from the repository root on a Big Red 200 login node:
#
#     ./run_entanglement_validation_br200.sh -A <slurm-account-name>
#
# This script
#   1. builds entanglement_validation with the Geant4 conda environment
#      (same compiler and geant4-config approach as build_smoke.sh);
#   2. submits a Slurm job array with one task per configuration
#      (native, direct, pickoff, legacy) x (entanglement on, off);
#   3. submits an analysis job that runs after the array succeeds.
#
# Settings (environment variables, all optional):
#   PSSOURCE_ENV        Geant4 conda environment
#                       (default /N/project/MIPHYS/QEPET/env)
#   RESULTS_DIR         output directory
#                       (default /N/scratch/$USER/pssource_entanglement_validation/<timestamp>)
#   PARTITION           Slurm partition (default general)
#   WORKERS             Geant4 processes per array task, one per core (default 128)
#   EVENTS_PER_WORKER   positron events per process (default 1000000)
#   WALLTIME            wall time per array task (default 01:00:00)
#   SEED_BASE           base random seed (default 20260928)
#
# With the defaults each configuration simulates 1.28e8 annihilations,
# giving a modulation uncertainty of about 0.003 in the 72-92 degree window.

set -euo pipefail

usage() {
    echo "Usage: $0 -A <slurm-account-name> [--build-only] [--no-build]" >&2
    exit 2
}

ACCOUNT=""
BUILD=1
SUBMIT=1

while [[ $# -gt 0 ]]; do
    case "$1" in
        -A|--account) ACCOUNT="${2:-}"; shift 2 ;;
        --build-only) SUBMIT=0; shift ;;
        --no-build) BUILD=0; shift ;;
        *) usage ;;
    esac
done

if [[ "$SUBMIT" -eq 1 && -z "$ACCOUNT" ]]; then
    echo "ERROR: a Slurm account is required (-A). Find it on the RT Projects home page." >&2
    usage
fi

export REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export PSSOURCE_ENV="${PSSOURCE_ENV:-/N/project/MIPHYS/QEPET/env}"
export RESULTS_DIR="${RESULTS_DIR:-/N/scratch/$USER/pssource_entanglement_validation/$(date +%Y%m%d_%H%M%S)}"
export WORKERS="${WORKERS:-128}"
export EVENTS_PER_WORKER="${EVENTS_PER_WORKER:-1000000}"
export SEED_BASE="${SEED_BASE:-20260928}"
PARTITION="${PARTITION:-general}"
WALLTIME="${WALLTIME:-01:00:00}"

BIN_DIR="$RESULTS_DIR/bin"
export VALIDATION_BIN="$BIN_DIR/entanglement_validation"

# shellcheck source=entanglement_validation_env.sh
source "$REPO_DIR/entanglement_validation_env.sh"
activate_pssource_env

mkdir -p "$BIN_DIR" "$RESULTS_DIR/logs"

if [[ "$BUILD" -eq 1 ]]; then
    echo "Building entanglement_validation with $(command -v "$CXX")"

    # shellcheck disable=SC2046
    "$CXX" -std=c++17 -O2 \
        $(geant4-config --cflags) \
        -I"$REPO_DIR" \
        "$REPO_DIR/entanglement_validation.cc" \
        "$REPO_DIR/ConfigurablePsModel.cc" \
        "$REPO_DIR/FixedParameterizedPsModel.cc" \
        "$REPO_DIR/OrePowellPsModel.cc" \
        "$REPO_DIR/PsTerminalStateBuilder.cc" \
        "$REPO_DIR/PsSourceAnnihilationProcess.cc" \
        "$REPO_DIR/PsSourceAnnihilationPhysics.cc" \
        $(geant4-config --libs) \
        -o "$VALIDATION_BIN"

    echo "Built $VALIDATION_BIN"
fi

[[ -x "$VALIDATION_BIN" ]] || { echo "ERROR: $VALIDATION_BIN not found; run without --no-build." >&2; exit 1; }

# Provenance for the results directory.
{
    echo "date: $(date -Iseconds)"
    echo "host: $(hostname)"
    echo "repo: $REPO_DIR"
    echo "git_commit: $(git -C "$REPO_DIR" rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "git_branch: $(git -C "$REPO_DIR" rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
    echo "git_dirty: $(git -C "$REPO_DIR" status --porcelain 2>/dev/null | wc -l | tr -d ' ') files"
    echo "geant4: $(geant4-config --version)"
    echo "compiler: $("$CXX" --version | head -1)"
    echo "workers_per_configuration: $WORKERS"
    echo "events_per_worker: $EVENTS_PER_WORKER"
    echo "seed_base: $SEED_BASE"
} > "$RESULTS_DIR/provenance.txt"

if [[ "$SUBMIT" -eq 0 ]]; then
    echo "Build only; nothing submitted."
    exit 0
fi

ARRAY_JOB=$(
    sbatch --parsable \
        -A "$ACCOUNT" \
        -p "$PARTITION" \
        -J pssource-entangle \
        --array=0-7 \
        --nodes=1 \
        --ntasks=1 \
        --cpus-per-task="$WORKERS" \
        --mem=64G \
        --time="$WALLTIME" \
        -o "$RESULTS_DIR/logs/array_%A_%a.out" \
        -e "$RESULTS_DIR/logs/array_%A_%a.err" \
        --export=ALL \
        "$REPO_DIR/entanglement_validation_br200.sbatch"
)

ANALYSIS_JOB=$(
    sbatch --parsable \
        -A "$ACCOUNT" \
        -p "$PARTITION" \
        -J pssource-entangle-analysis \
        --dependency=afterok:"$ARRAY_JOB" \
        --nodes=1 \
        --ntasks=1 \
        --cpus-per-task=1 \
        --mem=32G \
        --time=00:30:00 \
        -o "$RESULTS_DIR/logs/analysis_%j.out" \
        -e "$RESULTS_DIR/logs/analysis_%j.err" \
        --export=ALL \
        "$REPO_DIR/entanglement_validation_analysis_br200.sbatch"
)

echo "Results directory : $RESULTS_DIR"
echo "Simulation array  : $ARRAY_JOB (8 tasks)"
echo "Analysis job      : $ANALYSIS_JOB (runs after the array succeeds)"
echo
echo "Monitor with      : squeue -u $USER"
echo "Summary when done : cat $RESULTS_DIR/entanglement_validation_summary.txt"
