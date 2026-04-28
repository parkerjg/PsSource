#!/bin/bash

set -e

EXE=./ps_pointsource

# -------------------------------
# global run settings
# -------------------------------
BEAM=100000
IDEAL="on"

# geometry / recon params (keep fixed for paper)
HEAD_SIZE=25
HEAD_SEP=30
VOX=0.25
NX=101
NY=101

# output base
OUTDIR=pointsource_matrix
mkdir -p ${OUTDIR}

echo "Running 12-case point source matrix..."
echo "Output dir: ${OUTDIR}"
echo ""

# -------------------------------
# helper function
# -------------------------------
run_case () {

    NAME=$1
    METHOD=$2
    EXTRA_ARGS=$3

    DIR=${OUTDIR}/${NAME}_${METHOD}
    mkdir -p ${DIR}

    echo "=== RUN: ${NAME} | ${METHOD} ==="

    ${EXE} \
        --beam-on ${BEAM} \
        --recon-method ${METHOD} \
        --head-size-mm ${HEAD_SIZE} \
        --head-separation-mm ${HEAD_SEP} \
        --nx ${NX} --ny ${NY} \
        --voxel-size-mm ${VOX} \
        --ideal-acceptance ${IDEAL} \
        ${EXTRA_ARGS}

    # move outputs cleanly
    mv recon.csv            ${DIR}/
    mv recon.pgm            ${DIR}/
    mv profile_x.csv        ${DIR}/
    mv profile_y.csv        ${DIR}/
    mv fwhm.json            ${DIR}/
    mv recon_summary.json   ${DIR}/
    mv run_config.json      ${DIR}/

    echo "Saved -> ${DIR}"
    echo ""
}

# ============================================================
# SOURCE DEFINITIONS
# ============================================================

# S1: pure 2γ
S1="--generation-mode explicit --f-direct 0 --f-pps 0 --f-ops 1 --ortho-3g-fraction 0.0 --prompt off"

# S2: pure 3γ
S2="--generation-mode explicit --f-direct 0 --f-pps 0 --f-ops 1 --ortho-3g-fraction 1.0 --prompt off"

# S3: mixed
S3="--generation-mode explicit --f-direct 0.3 --f-pps 0.2 --f-ops 0.5 --ortho-3g-fraction 1.0 --prompt off"

# S4: mixed + prompt
S4="--generation-mode explicit --f-direct 0.3 --f-pps 0.2 --f-ops 0.5 --ortho-3g-fraction 1.0 --prompt on"

# ============================================================
# RUN MATRIX (4 sources × 3 methods = 12 runs)
# ============================================================

# ---- S1: pure 2γ ----
run_case "S1_pure2g" "lor2g"   "${S1}"
run_case "S1_pure2g" "cone3g"  "${S1}"
run_case "S1_pure2g" "unified" "${S1}"

# ---- S2: pure 3γ ----
run_case "S2_pure3g" "lor2g"   "${S2}"
run_case "S2_pure3g" "cone3g"  "${S2}"
run_case "S2_pure3g" "unified" "${S2}"

# ---- S3: mixed ----
run_case "S3_mixed" "lor2g"   "${S3}"
run_case "S3_mixed" "cone3g"  "${S3}"
run_case "S3_mixed" "unified" "${S3}"

# ---- S4: mixed + prompt ----
run_case "S4_mixed_prompt" "lor2g"   "${S4}"
run_case "S4_mixed_prompt" "cone3g"  "${S4}"
run_case "S4_mixed_prompt" "unified" "${S4}"

echo "======================================"
echo "12-case matrix COMPLETE"
echo "======================================"

