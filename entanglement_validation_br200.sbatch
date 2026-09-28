#!/bin/bash
#
# One array task of the entanglement validation. Submitted by
# run_entanglement_validation_br200.sh, which sets the account, partition,
# array range, resources, and these environment variables:
#   REPO_DIR, PSSOURCE_ENV, RESULTS_DIR, VALIDATION_BIN, WORKERS,
#   EVENTS_PER_WORKER, SEED_BASE
#
# Array index -> configuration:
#   0 native/on   1 native/off   2 direct/on    3 direct/off
#   4 pickoff/on  5 pickoff/off  6 legacy/on    7 legacy/off
#
# Each task runs WORKERS independent single-threaded Geant4 processes on one
# node, each with its own seed and output file.

set -euo pipefail

MODES=(native native direct direct pickoff pickoff legacy legacy)
QES=(on off on off on off on off)

TASK="${SLURM_ARRAY_TASK_ID:?must run as a Slurm array task}"
MODE="${MODES[$TASK]}"
QE="${QES[$TASK]}"

# shellcheck source=entanglement_validation_env.sh
source "$REPO_DIR/entanglement_validation_env.sh"
activate_pssource_env

OUT_DIR="$RESULTS_DIR/${MODE}_qe${QE}"
mkdir -p "$OUT_DIR"

echo "Task $TASK: mode=$MODE qe=$QE workers=$WORKERS events/worker=$EVENTS_PER_WORKER on $(hostname)"
echo "Geant4 $(geant4-config --version), binary $VALIDATION_BIN"

cd "$OUT_DIR"

for (( worker = 0; worker < WORKERS; ++worker )); do
    # Unique, reproducible seed per (configuration, worker).
    seed=$(( SEED_BASE + 1000000 * (TASK + 1) + worker ))

    "$VALIDATION_BIN" \
        --mode "$MODE" \
        --qe "$QE" \
        --events "$EVENTS_PER_WORKER" \
        --seed "$seed" \
        --record-theta-min 60 \
        --record-theta-max 105 \
        --out "$OUT_DIR/worker_$(printf '%04d' "$worker").csv" \
        > "$OUT_DIR/worker_$(printf '%04d' "$worker").log" 2>&1 &
done

# Wait for every worker and fail the task if any worker failed.
failures=0
for job in $(jobs -p); do
    wait "$job" || failures=$(( failures + 1 ))
done

summaries=$(grep -h "EntanglementValidation" "$OUT_DIR"/worker_*.log | wc -l)
echo "Completed workers: $summaries / $WORKERS, failures: $failures"

if [[ "$failures" -ne 0 || "$summaries" -ne "$WORKERS" ]]; then
    echo "ERROR: some workers failed; see $OUT_DIR/worker_*.log" >&2
    exit 1
fi

awk '
    /EntanglementValidation/ {
        for (i = 1; i <= NF; ++i) {
            split($i, kv, "=")
            if (kv[1] == "events") events += kv[2]
            if (kv[1] == "compton_pairs") pairs += kv[2]
            if (kv[1] == "written_pairs") written += kv[2]
        }
    }
    END {
        printf "Totals: events=%d compton_pairs=%d written_pairs=%d\n", events, pairs, written
    }
' "$OUT_DIR"/worker_*.log
