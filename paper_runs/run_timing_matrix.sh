#!/bin/bash
set -euo pipefail

EXE=./ps_main
OUTDIR=timing_matrix
mkdir -p "${OUTDIR}"

NREP=10
EVENTS=(10000 50000 100000)

run_case () {
    local NAME=$1
    local BEAM=$2
    local EXTRA_ARGS=$3
    local REP=$4

    local DIR=${OUTDIR}/${NAME}/n_${BEAM}/rep_$(printf "%02d" "${REP}")
    mkdir -p "${DIR}"

    local SEED1=$((300000 + 1000*REP + ${BEAM}))
    local SEED2=$((400000 + 1000*REP + ${BEAM}))

    echo "=== TIMING: ${NAME} | n=${BEAM} | rep ${REP} ==="

    /usr/bin/time -p -o "${DIR}/time.txt" \
        ${EXE} \
        --beam-on "${BEAM}" \
        --pre-cmd "/random/setSeeds ${SEED1} ${SEED2}" \
        ${EXTRA_ARGS} \
        > "${DIR}/run.log" 2>&1

    mv run_config.json "${DIR}/"
}

NATIVE="--preset native_reference --prompt off"
E2G="--generation-mode explicit --f-direct 0 --f-pps 0 --f-ops 1 --ortho-3g-fraction 0.0 --prompt off --positron-range off"
E3G="--generation-mode explicit --f-direct 0 --f-pps 0 --f-ops 1 --ortho-3g-fraction 1.0 --prompt off --positron-range off"
EMIX="--generation-mode explicit --f-direct 0.3 --f-pps 0.2 --f-ops 0.5 --ortho-3g-fraction 1.0 --prompt off --positron-range off"

for N in "${EVENTS[@]}"; do
    for REP in $(seq 1 ${NREP}); do
        run_case "native_reference" "${N}" "${NATIVE}" "${REP}"
        run_case "explicit_2g"      "${N}" "${E2G}"    "${REP}"
        run_case "explicit_3g"      "${N}" "${E3G}"    "${REP}"
        run_case "explicit_mixed"   "${N}" "${EMIX}"   "${REP}"
    done
done

echo "======================================"
echo "Timing matrix COMPLETE"
echo "======================================"
