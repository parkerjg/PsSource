#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")"
    pwd
)"

EVENT_COUNT="${1:-100}"
STATISTICAL_EVENT_COUNT="${2:-0}"
RUN_ROOT="${ROOT_DIR}/regression_runs/transport_coupled"

PS_TIMING="${ROOT_DIR}/ps_timing"
VALIDATOR="${ROOT_DIR}/validate_transport_coupled.py"

fail()
{
    echo
    echo "TRANSPORT REGRESSION FAILURE: $*" >&2
    exit 1
}

if [[ ! "${EVENT_COUNT}" =~ ^[1-9][0-9]*$ ]]; then
    fail "Event count must be a positive integer."
fi

if [[ ! "${STATISTICAL_EVENT_COUNT}" =~ ^[0-9]+$ ]]; then
    fail "Statistical event count must be a nonnegative integer."
fi

[[ -x "${PS_TIMING}" ]] ||
    fail "Executable not found: ${PS_TIMING}"

[[ -f "${VALIDATOR}" ]] ||
    fail "Validator not found: ${VALIDATOR}"

rm -rf "${RUN_ROOT}"
mkdir -p "${RUN_ROOT}"

run_case()
{
    local directory_name="$1"
    local validation_case="$2"
    local f_direct="$3"
    local f_pps="$4"
    local f_ops="$5"
    local ortho_3g_fraction="$6"
    local fixed_delay_ns="$7"

    local run_dir="${RUN_ROOT}/${directory_name}"

    echo
    echo "============================================================"
    echo "Case       : ${validation_case}"
    echo "Events     : ${EVENT_COUNT}"
    echo "Output     : ${run_dir}"
    echo "============================================================"

    mkdir -p "${run_dir}"

    if ! (
        cd "${run_dir}"

        "${PS_TIMING}" \
            --generation-mode transport-coupled \
            --beam-on "${EVENT_COUNT}" \
            --f-direct "${f_direct}" \
            --f-pps "${f_pps}" \
            --f-ops "${f_ops}" \
            --ortho-3g-fraction "${ortho_3g_fraction}" \
            --delay-mode fixed \
            --fixed-delay-ns "${fixed_delay_ns}" \
            --three-gamma-model approximate \
            --prompt off \
            --positron-range off \
            --positron-kev 0.0001 \
            > run.log 2>&1
    ); then
        echo
        echo "Last 40 lines of run.log:" >&2
        tail -n 40 "${run_dir}/run.log" >&2 || true
        fail "Simulation failed for ${validation_case}"
    fi

    [[ -s "${run_dir}/annihilation_summary.csv" ]] ||
        fail "Missing summary CSV for ${validation_case}"

    [[ -s "${run_dir}/annihilation_gammas.csv" ]] ||
        fail "Missing gamma CSV for ${validation_case}"

    if ! python "${VALIDATOR}" \
        "${run_dir}/annihilation_summary.csv" \
        "${run_dir}/annihilation_gammas.csv" \
        --case "${validation_case}" \
        --expected-events "${EVENT_COUNT}" \
        --json-out "${run_dir}/validation.json" \
        2>&1 | tee "${run_dir}/validation.log"
    then
        fail "Validation failed for ${validation_case}"
    fi
}

run_exponential_case()
{
    local directory_name="$1"
    local validation_case="$2"
    local f_pps="$3"
    local f_ops="$4"
    local ortho_3g_fraction="$5"
    local tau_pps_ns="$6"
    local tau_ops_ns="$7"

    local run_dir="${RUN_ROOT}/${directory_name}"

    echo
    echo "============================================================"
    echo "Statistical case : ${validation_case}"
    echo "Events           : ${STATISTICAL_EVENT_COUNT}"
    echo "Output           : ${run_dir}"
    echo "============================================================"

    mkdir -p "${run_dir}"

    if ! (
        cd "${run_dir}"

        "${PS_TIMING}" \
            --generation-mode transport-coupled \
            --beam-on "${STATISTICAL_EVENT_COUNT}" \
            --f-direct 0 \
            --f-pps "${f_pps}" \
            --f-ops "${f_ops}" \
            --ortho-3g-fraction "${ortho_3g_fraction}" \
            --delay-mode exponential \
            --tau-pps-ns "${tau_pps_ns}" \
            --tau-ops-ns "${tau_ops_ns}" \
            --three-gamma-model approximate \
            --prompt off \
            --positron-range off \
            --positron-kev 0.0001 \
            > run.log 2>&1
    ); then
        echo
        echo "Last 40 lines of run.log:" >&2
        tail -n 40 "${run_dir}/run.log" >&2 || true
        fail "Simulation failed for ${validation_case}"
    fi

    [[ -s "${run_dir}/annihilation_summary.csv" ]] ||
        fail "Missing summary CSV for ${validation_case}"

    [[ -s "${run_dir}/annihilation_gammas.csv" ]] ||
        fail "Missing gamma CSV for ${validation_case}"

    if ! python "${VALIDATOR}" \
        "${run_dir}/annihilation_summary.csv" \
        "${run_dir}/annihilation_gammas.csv" \
        --case "${validation_case}" \
        --expected-events "${STATISTICAL_EVENT_COUNT}" \
        --json-out "${run_dir}/validation.json" \
        2>&1 | tee "${run_dir}/validation.log"
    then
        fail "Validation failed for ${validation_case}"
    fi
}

run_case \
    "direct_2g_zero_delay" \
    "direct-2g-zero-delay" \
    1 0 0 0 0

run_case \
    "pps_2g_fixed_delay" \
    "pps-2g-fixed-delay" \
    0 1 0 0 3

run_case \
    "ops_2g_fixed_delay" \
    "ops-2g-fixed-delay" \
    0 0 1 0 3

run_case \
    "ops_3g_fixed_delay" \
    "ops-3g-fixed-delay" \
    0 0 1 1 3

if (( STATISTICAL_EVENT_COUNT > 0 )); then
    run_exponential_case \
        "pps_2g_exponential" \
        "pps-2g-exponential" \
        1 0 0 0.125 3.0

    run_exponential_case \
        "ops_3g_exponential" \
        "ops-3g-exponential" \
        0 1 1 0.125 3.0
fi

echo
echo "============================================================"
echo "Transport-coupled regression suite: PASS"
echo "Events per case: ${EVENT_COUNT}"
echo "Results directory: ${RUN_ROOT}"
echo
echo "  PASS  direct-2g-zero-delay"
echo "  PASS  pps-2g-fixed-delay"
echo "  PASS  ops-2g-fixed-delay"
echo "  PASS  ops-3g-fixed-delay"
if (( STATISTICAL_EVENT_COUNT > 0 )); then
    echo
    echo "Statistical events per case: ${STATISTICAL_EVENT_COUNT}"
    echo "  PASS  pps-2g-exponential"
    echo "  PASS  ops-3g-exponential"
else
    echo
    echo "Statistical cases: skipped"
    echo "Run with a second argument to enable them, for example:"
    echo "  ./run_transport_coupled_regression.sh 100 10000"
fi
echo "============================================================"
