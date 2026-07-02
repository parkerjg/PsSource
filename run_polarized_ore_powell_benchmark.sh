#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

EVENTS="${EVENTS:-100000}"
OUTPUT_DIR="${OUTPUT_DIR:-ore_powell_polarized_benchmark_run}"
ORDINARY_DIR="${ORDINARY_DIR:-ore_powell_benchmark_run}"

SEED1="${SEED1:-161803}"
SEED2="${SEED2:-141421}"

EXECUTABLE="${REPO_ROOT}/ps_timing"
BASIC_VALIDATOR="${REPO_ROOT}/validate_positronium_outputs.py"
POLARIZATION_VALIDATOR="${REPO_ROOT}/validate_polarization.py"
SCIENTIFIC_VALIDATOR="${REPO_ROOT}/validate_polarized_ore_powell.py"

OUTPUT_PATH="${REPO_ROOT}/${OUTPUT_DIR}"
ORDINARY_PATH="${REPO_ROOT}/${ORDINARY_DIR}"

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

require_executable "${EXECUTABLE}"
require_file "${BASIC_VALIDATOR}"
require_file "${POLARIZATION_VALIDATOR}"
require_file "${SCIENTIFIC_VALIDATOR}"

[[ "${EVENTS}" =~ ^[1-9][0-9]*$ ]] \
    || fail "EVENTS must be a positive integer"

[[ "${SEED1}" =~ ^[1-9][0-9]*$ ]] \
    || fail "SEED1 must be a positive integer"

[[ "${SEED2}" =~ ^[1-9][0-9]*$ ]] \
    || fail "SEED2 must be a positive integer"

require_file "${ORDINARY_PATH}/annihilation_gammas.csv"
require_file "${ORDINARY_PATH}/benchmark_manifest.json"

if [[ -e "${OUTPUT_PATH}" ]]; then
    fail "Output directory already exists: ${OUTPUT_PATH}"
fi

mkdir -p "${OUTPUT_PATH}"

echo "=== PsSource polarized Ore-Powell benchmark ==="
echo "Repository       : ${REPO_ROOT}"
echo "Output           : ${OUTPUT_PATH}"
echo "Ordinary baseline: ${ORDINARY_PATH}"
echo "Events           : ${EVENTS}"
echo "Seeds            : ${SEED1} ${SEED2}"
echo

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
    echo "ordinary_baseline=${ORDINARY_PATH}"
    echo "python=$(command -v python)"
    python --version

    if command -v geant4-config >/dev/null 2>&1; then
        echo "geant4_config=$(command -v geant4-config)"
        echo "geant4_version=$(geant4-config --version)"
        echo "geant4_prefix=$(geant4-config --prefix)"
    else
        echo "geant4_config=not_found"
    fi
} > "${OUTPUT_PATH}/environment.txt" 2>&1

cat > "${OUTPUT_PATH}/command.sh" <<EOF
"${EXECUTABLE}" \\
    --generation-mode explicit \\
    --beam-on "${EVENTS}" \\
    --f-direct 0 \\
    --f-pps 0 \\
    --f-ops 1 \\
    --ortho-3g-fraction 1 \\
    --three-gamma-model ore-powell-polarized \\
    --tau-ops-ns 3 \\
    --prompt off \\
    --pre-cmd "/random/setSeeds ${SEED1} ${SEED2}"
EOF

chmod +x "${OUTPUT_PATH}/command.sh"

echo "[1/5] Generating polarized Ore-Powell events"

(
    cd "${OUTPUT_PATH}"

    "${EXECUTABLE}" \
        --generation-mode explicit \
        --beam-on "${EVENTS}" \
        --f-direct 0 \
        --f-pps 0 \
        --f-ops 1 \
        --ortho-3g-fraction 1 \
        --three-gamma-model ore-powell-polarized \
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

echo "[2/5] Running general output validation"

python "${BASIC_VALIDATOR}" \
    --summary "${OUTPUT_PATH}/annihilation_summary.csv" \
    --gammas "${OUTPUT_PATH}/annihilation_gammas.csv" \
    --json-out "${OUTPUT_PATH}/output_validation.json" \
    > "${OUTPUT_PATH}/output_validation.log" 2>&1

echo "[3/5] Running fundamental polarization validation"

python "${POLARIZATION_VALIDATOR}" \
    "${OUTPUT_PATH}/annihilation_gammas.csv" \
    --expect-polarized \
    > "${OUTPUT_PATH}/polarization_validation.log" 2>&1

echo "[4/5] Running polarized Ore-Powell characterization"

python "${SCIENTIFIC_VALIDATOR}" \
    "${OUTPUT_PATH}/annihilation_gammas.csv" \
    --ordinary-csv \
        "${ORDINARY_PATH}/annihilation_gammas.csv" \
    --output-prefix \
        "${OUTPUT_PATH}/polarized_validation" \
    > "${OUTPUT_PATH}/polarized_validation.log" 2>&1

for required_output in \
    polarized_validation_summary.json \
    polarized_validation_metrics.csv \
    polarized_validation_histograms.csv \
    polarized_validation_event_correlations.csv
do
    require_file "${OUTPUT_PATH}/${required_output}"
done

echo "[5/5] Verifying benchmark artifacts"

python - \
    "${OUTPUT_PATH}" \
    "${ORDINARY_PATH}" \
    "${EVENTS}" \
    "${SEED1}" \
    "${SEED2}" <<'PY'
import json
import sys
from pathlib import Path

output = Path(sys.argv[1])
ordinary = Path(sys.argv[2])
expected_events = int(sys.argv[3])
seed1 = int(sys.argv[4])
seed2 = int(sys.argv[5])

required = [
    output / "run_config.json",
    output / "annihilation_summary.csv",
    output / "annihilation_gammas.csv",
    output / "output_validation.json",
    output / "polarized_validation_summary.json",
    output / "polarized_validation_metrics.csv",
    output / "polarized_validation_histograms.csv",
    output / "polarized_validation_event_correlations.csv",
    ordinary / "benchmark_manifest.json",
    ordinary / "annihilation_gammas.csv",
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

with (output / "run_config.json").open() as handle:
    config = json.load(handle)

expected_config = {
    "generation_mode": "explicit",
    "beam_on": expected_events,
    "three_gamma_model": "ore-powell-polarized",
    "f_direct": 0,
    "f_pps": 0,
    "f_ops": 1,
    "ortho_3g_fraction": 1,
    "has_prompt_gamma": False,
}

errors = []

for key, expected in expected_config.items():
    actual = config.get(key)

    if actual != expected:
        errors.append(
            f"{key}: actual={actual!r}, expected={expected!r}"
        )

if errors:
    print("Run configuration mismatch:")
    for error in errors:
        print(f"  - {error}")
    raise SystemExit(1)

with (
    output / "polarized_validation_summary.json"
).open() as handle:
    polarized = json.load(handle)

if polarized.get("status") != "PASS":
    raise SystemExit(
        "Polarized scientific validator did not report PASS"
    )

if int(polarized.get("events_analyzed", -1)) != expected_events:
    raise SystemExit(
        "Polarized validator event count does not match request"
    )

if int(polarized.get("photon_count", -1)) != 3 * expected_events:
    raise SystemExit(
        "Polarized photon count does not equal three per event"
    )

if int(polarized.get("invalid_polarization_count", -1)) != 0:
    raise SystemExit("Invalid polarization count was nonzero")

if int(polarized.get("multiplicity_failures", -1)) != 0:
    raise SystemExit("Multiplicity failures were nonzero")

fundamental = polarized["fundamental_vector_checks"]
comparison = polarized["ordinary_backend_comparison"]

if comparison is None:
    raise SystemExit("Ordinary-backend comparison was not performed")

cdf_difference = float(
    comparison["maximum_binned_energy_cdf_difference"]
)

cdf_tolerance = float(
    comparison["energy_cdf_tolerance"]
)

if cdf_difference > cdf_tolerance:
    raise SystemExit(
        f"Energy CDF difference {cdf_difference} "
        f"exceeds tolerance {cdf_tolerance}"
    )

manifest = {
    "status": "PASS",
    "events": expected_events,
    "photon_count": 3 * expected_events,
    "seed1": seed1,
    "seed2": seed2,
    "generation_mode": "explicit",
    "physics_backend": "ore-powell-polarized",
    "ordinary_baseline_directory": str(ordinary),
    "polarized_validation_status": polarized["status"],
    "maximum_direction_norm_error": (
        fundamental["maximum_direction_norm_error"]
    ),
    "maximum_polarization_norm_error": (
        fundamental["maximum_polarization_norm_error"]
    ),
    "maximum_abs_direction_dot_polarization": (
        fundamental[
            "maximum_abs_direction_dot_polarization"
        ]
    ),
    "ordinary_polarized_energy_cdf_difference": (
        cdf_difference
    ),
    "ordinary_polarized_energy_cdf_tolerance": (
        cdf_tolerance
    ),
    "mean_abs_polarization_dot_plane_normal": (
        polarized[
            "polarization_relative_to_decay_plane"
        ]["absolute_dot_with_plane_normal"]["mean"]
    ),
    "mean_abs_polarization_dot_global_z": (
        polarized[
            "polarization_relative_to_global_z_axis"
        ]["absolute_dot_with_z"]["mean"]
    ),
    "mean_pairwise_polarization_dot": (
        polarized[
            "pairwise_polarization_correlations"
        ]["all_signed_dots"]["mean"]
    ),
    "mean_abs_pairwise_polarization_dot": (
        polarized[
            "pairwise_polarization_correlations"
        ]["all_absolute_dots"]["mean"]
    ),
}

with (output / "benchmark_manifest.json").open("w") as handle:
    json.dump(manifest, handle, indent=2, sort_keys=True)
    handle.write("\n")

print("PASS")
PY

git -C "${REPO_ROOT}" status --short \
    > "${OUTPUT_PATH}/git_status_after.txt"

echo
echo "=== POLARIZED BENCHMARK PASS ==="
echo "Output directory:"
echo "  ${OUTPUT_PATH}"
echo
echo "Key results:"

python - "${OUTPUT_PATH}/benchmark_manifest.json" <<'PY'
import json
import sys

with open(sys.argv[1]) as handle:
    result = json.load(handle)

print(f"  Events                     : {result['events']}")
print(f"  Photons                    : {result['photon_count']}")
print(f"  Backend                    : {result['physics_backend']}")
print(
    "  Max polarization norm err : "
    f"{result['maximum_polarization_norm_error']:.6e}"
)
print(
    "  Max |direction dot pol|   : "
    f"{result['maximum_abs_direction_dot_polarization']:.6e}"
)
print(
    "  Ordinary/polarized CDF Δ  : "
    f"{result['ordinary_polarized_energy_cdf_difference']:.6e}"
)
print(f"  Status                     : {result['status']}")
PY
