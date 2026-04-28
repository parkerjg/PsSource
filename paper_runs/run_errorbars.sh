#!/usr/bin/env bash
set -euo pipefail

ROOT="paper_data_errorbars"
NREP=10
BEAM=5000

mkdir -p "$ROOT"/native_fraction_sweep
mkdir -p "$ROOT"/explicit_branch_sweep
mkdir -p "$ROOT"/lifetime_sweep

run_case() {
    local family="$1"
    local tag="$2"
    local rep="$3"
    shift 3

    local outdir="${ROOT}/${family}/${tag}/rep_$(printf '%02d' "$rep")"
    mkdir -p "$outdir"

    local seed1 seed2
    seed1=$((100000 + rep + 1000 * ${CASE_INDEX:-0}))
    seed2=$((200000 + rep + 1000 * ${CASE_INDEX:-0}))

    rm -f run_config.json hits_plus.csv hits_minus.csv annihilation_summary.csv annihilation_gammas.csv

    ./ps_timing \
        "$@" \
        --pre-cmd "/random/setSeeds ${seed1} ${seed2}" \
        > "${outdir}/run.log" 2>&1

    cp run_config.json            "${outdir}/"
    cp hits_plus.csv             "${outdir}/"
    cp hits_minus.csv            "${outdir}/"
    cp annihilation_summary.csv  "${outdir}/"
    cp annihilation_gammas.csv   "${outdir}/"

    python validate_positronium_outputs.py \
        --summary "${outdir}/annihilation_summary.csv" \
        --gammas  "${outdir}/annihilation_gammas.csv" \
        --json-out "${outdir}/validation.json" \
        > "${outdir}/validation.txt" 2>&1

    echo "${family}/${tag}/rep_$(printf '%02d' "$rep") done"
}

CASE_INDEX=0

# ------------------------------------------------------------------
# Native fraction sweep
# ------------------------------------------------------------------
for f in 0.00 0.25 0.50 0.75 1.00; do
    CASE_INDEX=$((CASE_INDEX + 1))
    tag=$(printf "f_%0.2f" "$f")
    for rep in $(seq 1 "$NREP"); do
        run_case native_fraction_sweep "$tag" "$rep" \
            --generation-mode native \
            --beam-on "$BEAM" \
            --at-rest-model OrePowellPolar \
            --world-material G4_AIR \
            --orto-ps-fraction "$f" \
            --prompt on
    done
done

# ------------------------------------------------------------------
# Explicit branch sweep
# ------------------------------------------------------------------
for g in 0.00 0.25 0.50 0.75 1.00; do
    CASE_INDEX=$((CASE_INDEX + 1))
    tag=$(printf "g_%0.2f" "$g")
    for rep in $(seq 1 "$NREP"); do
        run_case explicit_branch_sweep "$tag" "$rep" \
            --generation-mode explicit \
            --beam-on "$BEAM" \
            --f-direct 0 \
            --f-pps 0 \
            --f-ops 1 \
            --ortho-3g-fraction "$g" \
            --tau-ops-ns 3.0 \
            --prompt off \
            --positron-range off
    done
done

# ------------------------------------------------------------------
# Lifetime sweep
# ------------------------------------------------------------------
for tau in 1.0 3.0 5.0 10.0; do
    CASE_INDEX=$((CASE_INDEX + 1))
    tag=$(printf "tau_%0.1f" "$tau")
    for rep in $(seq 1 "$NREP"); do
        run_case lifetime_sweep "$tag" "$rep" \
            --preset explicit_3g_dev \
            --beam-on "$BEAM" \
            --tau-ops-ns "$tau"
    done
done

echo "All error-bar runs completed."
