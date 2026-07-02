#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

EVENTS="${EVENTS:-100000}"
OUTPUT_DIR="${OUTPUT_DIR:-native_geant4_ore_powell_run}"
SEED1="${SEED1:-271828}"
SEED2="${SEED2:-314159}"

GRID_BINS="${GRID_BINS:-50}"
GRID_SUBDIVISIONS="${GRID_SUBDIVISIONS:-12}"
MIN_EXPECTED_COUNT="${MIN_EXPECTED_COUNT:-5}"

REFERENCE_DIR="${REPO_ROOT}/reference/native_geant4_ore_powell"
BUILD_SCRIPT="${REFERENCE_DIR}/build.sh"
EXECUTABLE="${REFERENCE_DIR}/native_geant4_ore_powell"

ANALYTIC_VALIDATOR="${REPO_ROOT}/validate_ore_powell_reference.py"
GEOMETRY_VALIDATOR="${REPO_ROOT}/validate_three_gamma_geometry.py"

OUTPUT_PATH="${REPO_ROOT}/${OUTPUT_DIR}"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

require_file() {
    [[ -f "$1" ]] || fail "Required file not found: $1"
}

require_executable() {
    [[ -x "$1" ]] || fail "Required executable not found: $1"
}

require_file "${BUILD_SCRIPT}"
require_file "${ANALYTIC_VALIDATOR}"
require_file "${GEOMETRY_VALIDATOR}"

[[ "${EVENTS}" =~ ^[1-9][0-9]*$ ]] \
    || fail "EVENTS must be a positive integer"

[[ "${SEED1}" =~ ^[1-9][0-9]*$ ]] \
    || fail "SEED1 must be a positive integer"

[[ "${SEED2}" =~ ^[1-9][0-9]*$ ]] \
    || fail "SEED2 must be a positive integer"

[[ "${GRID_BINS}" =~ ^[1-9][0-9]*$ ]] \
    || fail "GRID_BINS must be a positive integer"

[[ "${GRID_SUBDIVISIONS}" =~ ^[1-9][0-9]*$ ]] \
    || fail "GRID_SUBDIVISIONS must be a positive integer"

if [[ -e "${OUTPUT_PATH}" ]]; then
    fail "Output directory already exists: ${OUTPUT_PATH}"
fi

mkdir -p "${OUTPUT_PATH}"

echo "=== Native Geant4 Ore-Powell benchmark ==="
echo "Repository : ${REPO_ROOT}"
echo "Output     : ${OUTPUT_PATH}"
echo "Events     : ${EVENTS}"
echo "Seeds      : ${SEED1} ${SEED2}"
echo "Grid       : ${GRID_BINS} bins, ${GRID_SUBDIVISIONS} subdivisions"
echo "Min count  : ${MIN_EXPECTED_COUNT}"
echo

echo "[1/5] Building native Geant4 reference"

bash "${BUILD_SCRIPT}" \
    > "${OUTPUT_PATH}/build.log" 2>&1

require_executable "${EXECUTABLE}"

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
    echo "grid_bins=${GRID_BINS}"
    echo "grid_subdivisions=${GRID_SUBDIVISIONS}"
    echo "minimum_expected_count=${MIN_EXPECTED_COUNT}"
    echo "compiler=${CXX:-default}"
    echo "python=$(command -v python)"
    python --version

    if command -v geant4-config >/dev/null 2>&1; then
        echo "geant4_version=$(geant4-config --version)"
        echo "geant4_prefix=$(geant4-config --prefix)"
        echo "geant4_config=$(command -v geant4-config)"
    else
        echo "geant4_config=not_found"
    fi
} > "${OUTPUT_PATH}/environment.txt" 2>&1

cat > "${OUTPUT_PATH}/command.sh" <<EOF
"${EXECUTABLE}" \\
    --events "${EVENTS}" \\
    --seed1 "${SEED1}" \\
    --seed2 "${SEED2}" \\
    --output annihilation_gammas.csv
EOF

chmod +x "${OUTPUT_PATH}/command.sh"

echo "[2/5] Generating native Geant4 Ore-Powell events"

(
    cd "${OUTPUT_PATH}"

    "${EXECUTABLE}" \
        --events "${EVENTS}" \
        --seed1 "${SEED1}" \
        --seed2 "${SEED2}" \
        --output annihilation_gammas.csv
) > "${OUTPUT_PATH}/simulation.log" 2>&1

require_file "${OUTPUT_PATH}/annihilation_gammas.csv"

echo "[3/5] Running independent analytic Ore-Powell validation"

python "${ANALYTIC_VALIDATOR}" \
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

echo "[4/5] Running geometry and isotropy validation"

python "${GEOMETRY_VALIDATOR}" \
    "${OUTPUT_PATH}/annihilation_gammas.csv" \
    --output-prefix "${OUTPUT_PATH}/three_gamma_geometry" \
    > "${OUTPUT_PATH}/three_gamma_geometry.log" 2>&1

require_file "${OUTPUT_PATH}/three_gamma_geometry_summary.json"
require_file "${OUTPUT_PATH}/three_gamma_geometry_metrics.csv"
require_file "${OUTPUT_PATH}/three_gamma_geometry_histograms.csv"

echo "[5/5] Verifying benchmark artifacts"

python - \
    "${OUTPUT_PATH}" \
    "${EVENTS}" \
    "${SEED1}" \
    "${SEED2}" <<'PY'
import csv
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
expected_events = int(sys.argv[2])
seed1 = int(sys.argv[3])
seed2 = int(sys.argv[4])

required = [
    root / "annihilation_gammas.csv",
    root / "ore_powell_reference_summary.json",
    root / "ore_powell_reference_grid.csv",
    root / "three_gamma_geometry_summary.json",
    root / "three_gamma_geometry_metrics.csv",
    root / "three_gamma_geometry_histograms.csv",
]

missing = [
    str(path)
    for path in required
    if not path.is_file() or path.stat().st_size == 0
]

if missing:
    print("Missing or empty artifacts:")
    for path in missing:
        print(f"  - {path}")
    raise SystemExit(1)

event_counts = {}

with (root / "annihilation_gammas.csv").open() as handle:
    reader = csv.DictReader(handle)

    for row in reader:
        event_id = int(row["event_id"])
        event_counts[event_id] = event_counts.get(event_id, 0) + 1

if len(event_counts) != expected_events:
    raise SystemExit(
        f"Found {len(event_counts)} events, expected {expected_events}"
    )

multiplicity_failures = sum(
    count != 3
    for count in event_counts.values()
)

if multiplicity_failures != 0:
    raise SystemExit(
        f"Multiplicity failures: {multiplicity_failures}"
    )

with (
    root / "ore_powell_reference_summary.json"
).open() as handle:
    analytic = json.load(handle)

if analytic.get("status") != "PASS":
    raise SystemExit(
        "Independent Ore-Powell validator did not report PASS"
    )

if int(analytic.get("events_analyzed", -1)) != expected_events:
    raise SystemExit(
        "Analytic validator event count mismatch"
    )

with (
    root / "three_gamma_geometry_summary.json"
).open() as handle:
    geometry = json.load(handle)

if geometry.get("status") != "PASS":
    raise SystemExit(
        "Geometry validator did not report PASS"
    )

if int(geometry.get("events_analyzed", -1)) != expected_events:
    raise SystemExit(
        "Geometry validator event count mismatch"
    )

manifest = {
    "status": "PASS",
    "events": expected_events,
    "photon_count": 3 * expected_events,
    "seed1": seed1,
    "seed2": seed2,
    "physics_backend": "native_G4OrePowellAtRestModel",
    "analytic_validation_status": analytic["status"],
    "geometry_validation_status": geometry["status"],
    "chi_square": analytic["chi_square"],
    "degrees_of_freedom": analytic["degrees_of_freedom"],
    "chi_square_z_score": analytic["chi_square_z_score"],
    "maximum_marginal_cdf_difference": (
        analytic["maximum_marginal_cdf_difference"]
    ),
    "maximum_standardized_residual": (
        analytic["maximum_standardized_residual"]
    ),
    "maximum_cartesian_axis_mean_difference": (
        geometry["cartesian_axis_statistics"][
            "maximum_mean_difference"
        ]
    ),
    "photon_cos_theta_z_score": (
        geometry["photon_cos_theta"]["z_score"]
    ),
    "photon_phi_z_score": (
        geometry["photon_phi"]["z_score"]
    ),
    "plane_normal_cos_theta_z_score": (
        geometry["plane_normal_cos_theta"]["z_score"]
    ),
    "plane_normal_phi_z_score": (
        geometry["plane_normal_phi"]["z_score"]
    ),
    "maximum_coplanarity_error": (
        geometry["coplanarity"][
            "maximum_absolute_scalar_triple_product"
        ]
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
echo "=== NATIVE GEANT4 BENCHMARK PASS ==="
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
print(f"  Photons               : {result['photon_count']}")
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
