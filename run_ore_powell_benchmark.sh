#!/usr/bin/env bash
set -euo pipefail

# Full scientific benchmark for the ordinary, unpolarized Ore-Powell backend.
#
# Default:
#   bash run_ore_powell_benchmark.sh
#
# Optional:
#   EVENTS=200000 OUTPUT_DIR=my_benchmark SEED1=12345 SEED2=67890 \
#       bash run_ore_powell_benchmark.sh

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

EVENTS="${EVENTS:-100000}"
OUTPUT_DIR="${OUTPUT_DIR:-ore_powell_benchmark_run}"
SEED1="${SEED1:-314159}"
SEED2="${SEED2:-271828}"
GRID_BINS="${GRID_BINS:-50}"
GRID_SUBDIVISIONS="${GRID_SUBDIVISIONS:-12}"
MIN_EXPECTED_COUNT="${MIN_EXPECTED_COUNT:-5}"

EXECUTABLE="${REPO_ROOT}/ps_timing"
OUTPUT_VALIDATOR="${REPO_ROOT}/validate_positronium_outputs.py"
REFERENCE_VALIDATOR="${REPO_ROOT}/validate_ore_powell_reference.py"
FIGURE_EXPORTER="${REPO_ROOT}/export_ore_powell_figure_data.py"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

require_file() {
    [[ -f "$1" ]] || fail "Required file not found: $1"
}

require_executable() {
    [[ -x "$1" ]] || fail "Required executable not found or not executable: $1"
}

require_executable "${EXECUTABLE}"
require_file "${OUTPUT_VALIDATOR}"
require_file "${REFERENCE_VALIDATOR}"
require_file "${FIGURE_EXPORTER}"

[[ "${EVENTS}" =~ ^[1-9][0-9]*$ ]] \
    || fail "EVENTS must be a positive integer"

[[ "${SEED1}" =~ ^[1-9][0-9]*$ ]] \
    || fail "SEED1 must be a positive integer"

[[ "${SEED2}" =~ ^[1-9][0-9]*$ ]] \
    || fail "SEED2 must be a positive integer"

OUTPUT_PATH="${REPO_ROOT}/${OUTPUT_DIR}"

if [[ -e "${OUTPUT_PATH}" ]]; then
    fail "Output directory already exists: ${OUTPUT_PATH}"
fi

mkdir -p "${OUTPUT_PATH}/figure_data"

echo "=== PsSource full Ore-Powell benchmark ==="
echo "Repository : ${REPO_ROOT}"
echo "Output     : ${OUTPUT_PATH}"
echo "Events     : ${EVENTS}"
echo "Seeds      : ${SEED1} ${SEED2}"
echo "Grid       : ${GRID_BINS} bins, ${GRID_SUBDIVISIONS} subdivisions"
echo "Min count  : ${MIN_EXPECTED_COUNT}"
echo

# -------------------------------------------------------------------------
# Record repository and environment metadata before running.
# -------------------------------------------------------------------------

git -C "${REPO_ROOT}" rev-parse HEAD \
    > "${OUTPUT_PATH}/git_commit.txt"

git -C "${REPO_ROOT}" status --short \
    > "${OUTPUT_PATH}/git_status_before.txt"

git -C "${REPO_ROOT}" log -1 --format=fuller \
    > "${OUTPUT_PATH}/git_commit_details.txt"

{
    echo "benchmark_timestamp_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "hostname=$(hostname)"
    echo "working_directory=${REPO_ROOT}"
    echo "events=${EVENTS}"
    echo "seed1=${SEED1}"
    echo "seed2=${SEED2}"
    echo "python=$(command -v python)"
    python --version
    if command -v geant4-config >/dev/null 2>&1; then
        echo "geant4_config=$(command -v geant4-config)"
        echo "geant4_version=$(geant4-config --version)"
    else
        echo "geant4_config=not_found"
    fi
} > "${OUTPUT_PATH}/environment.txt" 2>&1

# Save the exact command in shell-readable form.
cat > "${OUTPUT_PATH}/command.sh" <<EOF
"${EXECUTABLE}" \\
    --generation-mode explicit \\
    --beam-on "${EVENTS}" \\
    --f-direct 0 \\
    --f-pps 0 \\
    --f-ops 1 \\
    --ortho-3g-fraction 1 \\
    --three-gamma-model ore-powell \\
    --tau-ops-ns 3 \\
    --prompt off \\
    --pre-cmd "/random/setSeeds ${SEED1} ${SEED2}"
EOF

chmod +x "${OUTPUT_PATH}/command.sh"

# -------------------------------------------------------------------------
# Run simulation inside the benchmark directory so all generated files
# remain isolated.
# -------------------------------------------------------------------------

echo "[1/5] Generating ${EVENTS} ordinary Ore-Powell events"

(
    cd "${OUTPUT_PATH}"

    "${EXECUTABLE}" \
        --generation-mode explicit \
        --beam-on "${EVENTS}" \
        --f-direct 0 \
        --f-pps 0 \
        --f-ops 1 \
        --ortho-3g-fraction 1 \
        --three-gamma-model ore-powell \
        --tau-ops-ns 3 \
        --prompt off \
        --pre-cmd "/random/setSeeds ${SEED1} ${SEED2}"
) > "${OUTPUT_PATH}/simulation.log" 2>&1

for required_output in \
    run_config.json \
    annihilation_summary.csv \
    annihilation_gammas.csv
do
    require_file "${OUTPUT_PATH}/${required_output}"
done

# -------------------------------------------------------------------------
# Generic output validation.
# -------------------------------------------------------------------------

echo "[2/5] Running general output and closure validation"

python "${OUTPUT_VALIDATOR}" \
    --summary "${OUTPUT_PATH}/annihilation_summary.csv" \
    --gammas "${OUTPUT_PATH}/annihilation_gammas.csv" \
    --json-out "${OUTPUT_PATH}/output_validation.json" \
    > "${OUTPUT_PATH}/output_validation.log" 2>&1

# Explicitly enforce the generic-validator quantities that must be zero.
python - "${OUTPUT_PATH}/output_validation.json" "${EVENTS}" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
expected_events = int(sys.argv[2])

with path.open() as handle:
    report = json.load(handle)

mode = report["mode_summary"]
closure = report["closure_summary"]

failures = []

if mode["total_events"] != expected_events:
    failures.append(
        f"total_events={mode['total_events']} expected={expected_events}"
    )

if mode["annihilation_found_events"] != expected_events:
    failures.append(
        "annihilation_found_events="
        f"{mode['annihilation_found_events']} expected={expected_events}"
    )

mode_counts = {
    int(key): int(value)
    for key, value in mode["annihilation_mode_counts"].items()
}

if mode_counts != {3: expected_events}:
    failures.append(
        f"annihilation_mode_counts={mode_counts}, expected only mode 3"
    )

gamma_counts = {
    int(key): int(value)
    for key, value in mode["n_annihilation_gammas_counts"].items()
}

if gamma_counts != {3: expected_events}:
    failures.append(
        f"n_annihilation_gammas_counts={gamma_counts}, expected only 3"
    )

if closure["missing_gamma_events"] != 0:
    failures.append(
        f"missing_gamma_events={closure['missing_gamma_events']}"
    )

if closure["multiplicity_mismatch_events"] != 0:
    failures.append(
        "multiplicity_mismatch_events="
        f"{closure['multiplicity_mismatch_events']}"
    )

if failures:
    print("FAIL")
    for failure in failures:
        print(f"  - {failure}")
    raise SystemExit(1)

print("PASS")
PY

# -------------------------------------------------------------------------
# Independent analytic Ore-Powell validation.
# -------------------------------------------------------------------------

echo "[3/5] Running independent analytic Ore-Powell validation"

python "${REFERENCE_VALIDATOR}" \
    "${OUTPUT_PATH}/annihilation_gammas.csv" \
    --bins "${GRID_BINS}" \
    --subdivisions "${GRID_SUBDIVISIONS}" \
    --minimum-expected-count "${MIN_EXPECTED_COUNT}" \
    --chi-square-sigma-limit 6 \
    --cdf-factor 6 \
    --energy-sum-tolerance 5e-6 \
    --angle-tolerance 1e-5 \
    --angle-energy-floor 1e-3 \
    --output-prefix "${OUTPUT_PATH}/ore_powell_reference" \
    > "${OUTPUT_PATH}/ore_powell_reference.log" 2>&1

require_file "${OUTPUT_PATH}/ore_powell_reference_summary.json"
require_file "${OUTPUT_PATH}/ore_powell_reference_grid.csv"

# -------------------------------------------------------------------------
# Export publication/Excel data.
# -------------------------------------------------------------------------

echo "[4/5] Exporting Excel-ready validation data"

python "${FIGURE_EXPORTER}" \
    --gammas "${OUTPUT_PATH}/annihilation_gammas.csv" \
    --grid "${OUTPUT_PATH}/ore_powell_reference_grid.csv" \
    --summary "${OUTPUT_PATH}/ore_powell_reference_summary.json" \
    --output-dir "${OUTPUT_PATH}/figure_data" \
    > "${OUTPUT_PATH}/figure_export.log" 2>&1

# -------------------------------------------------------------------------
# Final cross-check and manifest.
# -------------------------------------------------------------------------

echo "[5/5] Verifying benchmark artifacts"

python - "${OUTPUT_PATH}" "${EVENTS}" "${SEED1}" "${SEED2}" <<'PY'
import csv
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
expected_events = int(sys.argv[2])
seed1 = int(sys.argv[3])
seed2 = int(sys.argv[4])

required = [
    root / "run_config.json",
    root / "annihilation_summary.csv",
    root / "annihilation_gammas.csv",
    root / "output_validation.json",
    root / "ore_powell_reference_summary.json",
    root / "ore_powell_reference_grid.csv",
    root / "figure_data" / "phase_space_long.csv",
    root / "figure_data" / "observed_density_matrix.csv",
    root / "figure_data" / "expected_density_matrix.csv",
    root / "figure_data" / "residual_matrix.csv",
    root / "figure_data" / "single_photon_spectrum.csv",
    root / "figure_data" / "ordered_energy_spectra.csv",
    root / "figure_data" / "opening_angle_spectra.csv",
    root / "figure_data" / "validation_summary.csv",
]

missing = [
    str(path)
    for path in required
    if not path.is_file() or path.stat().st_size == 0
]

if missing:
    print("Missing or empty benchmark artifacts:")
    for path in missing:
        print(f"  - {path}")
    raise SystemExit(1)

with (root / "run_config.json").open() as handle:
    config = json.load(handle)

expected_config = {
    "generation_mode": "explicit",
    "beam_on": expected_events,
    "three_gamma_model": "ore-powell",
    "f_direct": 0,
    "f_pps": 0,
    "f_ops": 1,
    "ortho_3g_fraction": 1,
    "has_prompt_gamma": False,
}

config_failures = []

for key, expected in expected_config.items():
    actual = config.get(key)
    if actual != expected:
        config_failures.append(
            f"{key}: actual={actual!r}, expected={expected!r}"
        )

if config_failures:
    print("Run configuration mismatch:")
    for failure in config_failures:
        print(f"  - {failure}")
    raise SystemExit(1)

with (root / "ore_powell_reference_summary.json").open() as handle:
    reference = json.load(handle)

if reference.get("status") != "PASS":
    raise SystemExit(
        "Independent Ore-Powell validator did not report PASS"
    )

if int(reference.get("events_analyzed", -1)) != expected_events:
    raise SystemExit(
        "Independent validator event count does not match requested count"
    )

if int(reference.get("multiplicity_failures", -1)) != 0:
    raise SystemExit("Multiplicity failures were nonzero")

if int(reference.get("phase_space_failures", -1)) != 0:
    raise SystemExit("Phase-space failures were nonzero")

with (
    root / "figure_data" / "single_photon_spectrum.csv"
).open() as handle:
    rows = list(csv.DictReader(handle))

photon_total = sum(int(row["observed_count"]) for row in rows)

if photon_total != 3 * expected_events:
    raise SystemExit(
        f"Photon histogram total {photon_total} != {3 * expected_events}"
    )

manifest = {
    "status": "PASS",
    "events": expected_events,
    "expected_photons": 3 * expected_events,
    "seed1": seed1,
    "seed2": seed2,
    "physics_backend": "ore-powell",
    "generation_mode": "explicit",
    "independent_validation_status": reference["status"],
    "chi_square": reference["chi_square"],
    "degrees_of_freedom": reference["degrees_of_freedom"],
    "chi_square_z_score": reference["chi_square_z_score"],
    "maximum_marginal_cdf_difference": (
        reference["maximum_marginal_cdf_difference"]
    ),
    "maximum_standardized_residual": (
        reference["maximum_standardized_residual"]
    ),
}

with (root / "benchmark_manifest.json").open("w") as handle:
    json.dump(manifest, handle, indent=2, sort_keys=True)
    handle.write("\n")

print("PASS")
PY

git -C "${REPO_ROOT}" status --short \
    > "${OUTPUT_PATH}/git_status_after.txt"

echo
echo "=== BENCHMARK PASS ==="
echo "Output directory:"
echo "  ${OUTPUT_PATH}"
echo
echo "Key results:"
python - "${OUTPUT_PATH}/benchmark_manifest.json" <<'PY'
import json
import sys

with open(sys.argv[1]) as handle:
    result = json.load(handle)

print(f"  Events                : {result['events']}")
print(f"  Backend               : {result['physics_backend']}")
print(f"  Chi-square            : {result['chi_square']:.6f}")
print(f"  Degrees of freedom    : {result['degrees_of_freedom']}")
print(f"  Chi-square z score    : {result['chi_square_z_score']:.6f}")
print(
    "  Maximum marginal CDF : "
    f"{result['maximum_marginal_cdf_difference']:.6e}"
)
print(f"  Status                : {result['status']}")
PY
