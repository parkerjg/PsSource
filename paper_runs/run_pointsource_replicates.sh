#!/bin/bash
set -euo pipefail

EXE=./ps_pointsource
OUTDIR=pointsource_replicates
mkdir -p "${OUTDIR}"

IDEAL="on"
HEAD_SIZE=25
HEAD_SEP=30
VOX=0.25
NX=101
NY=101

NREP=5

run_case () {
    local NAME=$1
    local METHOD=$2
    local BEAM=$3
    local EXTRA_ARGS=$4
    local REP=$5

    local DIR=${OUTDIR}/${NAME}/${METHOD}/rep_$(printf "%02d" "${REP}")
    mkdir -p "${DIR}"

    local SEED1=$((100000 + 1000*REP + ${BEAM}))
    local SEED2=$((200000 + 1000*REP + ${BEAM}))

    echo "=== RUN: ${NAME} | ${METHOD} | rep ${REP} | beam ${BEAM} ==="

    ${EXE} \
        --beam-on "${BEAM}" \
        --recon-method "${METHOD}" \
        --head-size-mm "${HEAD_SIZE}" \
        --head-separation-mm "${HEAD_SEP}" \
        --nx "${NX}" --ny "${NY}" \
        --voxel-size-mm "${VOX}" \
        --ideal-acceptance "${IDEAL}" \
        --pre-cmd "/random/setSeeds ${SEED1} ${SEED2}" \
        ${EXTRA_ARGS}

    mv recon.csv          "${DIR}/"
    mv recon.pgm          "${DIR}/"
    mv profile_x.csv      "${DIR}/"
    mv profile_y.csv      "${DIR}/"
    mv fwhm.json          "${DIR}/"
    mv recon_summary.json "${DIR}/"
    mv run_config.json    "${DIR}/"

    echo "Saved -> ${DIR}"
    echo ""
}

S1="--generation-mode explicit --f-direct 0 --f-pps 0 --f-ops 1 --ortho-3g-fraction 0.0 --prompt off"
S2="--generation-mode explicit --f-direct 0 --f-pps 0 --f-ops 1 --ortho-3g-fraction 1.0 --prompt off"
S3="--generation-mode explicit --f-direct 0.3 --f-pps 0.2 --f-ops 0.5 --ortho-3g-fraction 1.0 --prompt off"
S4="--generation-mode explicit --f-direct 0.3 --f-pps 0.2 --f-ops 0.5 --ortho-3g-fraction 1.0 --prompt on"

for REP in $(seq 1 ${NREP}); do
    # S1 pure 2g
    run_case "S1_pure2g"        "lor2g"   100000 "${S1}" "${REP}"
    run_case "S1_pure2g"        "cone3g"  100000 "${S1}" "${REP}"
    run_case "S1_pure2g"        "unified" 100000 "${S1}" "${REP}"

    # S2 pure 3g
    run_case "S2_pure3g"        "lor2g"   200000 "${S2}" "${REP}"
    run_case "S2_pure3g"        "cone3g"  200000 "${S2}" "${REP}"
    run_case "S2_pure3g"        "unified" 200000 "${S2}" "${REP}"

    # S3 mixed
    run_case "S3_mixed"         "lor2g"   150000 "${S3}" "${REP}"
    run_case "S3_mixed"         "cone3g"  150000 "${S3}" "${REP}"
    run_case "S3_mixed"         "unified" 150000 "${S3}" "${REP}"

    # S4 mixed + prompt
    run_case "S4_mixed_prompt"  "lor2g"   150000 "${S4}" "${REP}"
    run_case "S4_mixed_prompt"  "cone3g"  150000 "${S4}" "${REP}"
    run_case "S4_mixed_prompt"  "unified" 150000 "${S4}" "${REP}"
done

echo "======================================"
echo "Replicate matrix COMPLETE"
echo "======================================"
