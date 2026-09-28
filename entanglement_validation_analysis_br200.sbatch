#!/bin/bash
#
# Analysis step of the entanglement validation. Submitted by
# run_entanglement_validation_br200.sh with a dependency on the simulation
# array; uses REPO_DIR, PSSOURCE_ENV, and RESULTS_DIR from the environment.

set -euo pipefail

# shellcheck source=entanglement_validation_env.sh
source "$REPO_DIR/entanglement_validation_env.sh"
activate_pssource_env

python "$REPO_DIR/validate_entanglement_modulation.py" \
    "$RESULTS_DIR" \
    --json "$RESULTS_DIR/entanglement_validation.json" \
    --plot "$RESULTS_DIR/entanglement_validation.png" \
    | tee "$RESULTS_DIR/entanglement_validation_summary.txt"
