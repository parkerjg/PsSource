#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")"
    pwd
)"

REPO_ROOT="$(
    cd "${SCRIPT_DIR}/../.."
    pwd
)"

GATE_SIF="${GATE_SIF:-/N/project/MIPHYS/QEPET/gate.sif}"
OUTPUT_DIR="${OUTPUT_DIR:-${REPO_ROOT}/gate_ore_powell_run}"

MACRO="${SCRIPT_DIR}/gate_ore_powell.mac"
MATERIAL_DATABASE="${SCRIPT_DIR}/GateMaterials.db"
CONVERTER="${SCRIPT_DIR}/convert_gate_phase_space.py"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

[[ -f "${GATE_SIF}" ]] || fail "GATE image not found: ${GATE_SIF}"
[[ -f "${MACRO}" ]] || fail "Macro not found: ${MACRO}"
[[ -f "${MATERIAL_DATABASE}" ]] \
    || fail "Material database not found: ${MATERIAL_DATABASE}"
[[ -f "${CONVERTER}" ]] || fail "Converter not found: ${CONVERTER}"

command -v apptainer >/dev/null 2>&1 \
    || fail "apptainer was not found"

[[ -n "${CONDA_PREFIX:-}" ]] \
    || fail "CONDA_PREFIX is not set"

G4_DATA_DIR="${CONDA_PREFIX}/share/Geant4/data"

[[ -d "${G4_DATA_DIR}" ]] \
    || fail "Geant4 data directory not found: ${G4_DATA_DIR}"

[[ -n "${G4ENSDFSTATEDATA:-}" ]] \
    || fail "G4ENSDFSTATEDATA is not set"

[[ -f "${G4ENSDFSTATEDATA}/ENSDFSTATE.dat" ]] \
    || fail "ENSDFSTATE.dat not found: ${G4ENSDFSTATEDATA}/ENSDFSTATE.dat"

if [[ -e "${OUTPUT_DIR}" ]]; then
    fail "Output directory already exists: ${OUTPUT_DIR}"
fi

mkdir -p "${OUTPUT_DIR}"

cp "${MACRO}" "${OUTPUT_DIR}/gate_ore_powell.mac"
cp "${MATERIAL_DATABASE}" "${OUTPUT_DIR}/GateMaterials.db"

sha256sum "${GATE_SIF}" \
    > "${OUTPUT_DIR}/gate_sif_sha256.txt"

apptainer inspect --json "${GATE_SIF}" \
    > "${OUTPUT_DIR}/gate_sif_inspect.json"

{
    echo "timestamp_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "hostname=$(hostname)"
    echo "gate_sif=${GATE_SIF}"
    echo "apptainer=$(command -v apptainer)"
    apptainer --version
    echo "git_commit=$(git -C "${REPO_ROOT}" rev-parse HEAD)"
} > "${OUTPUT_DIR}/environment.txt" 2>&1

echo "=== GATE Ore-Powell benchmark ==="
echo "Container : ${GATE_SIF}"
echo "Output    : ${OUTPUT_DIR}"

if ! apptainer run \
    --bind "${OUTPUT_DIR}:/job" \
    --bind "${CONDA_PREFIX}/share/Geant4/data:${CONDA_PREFIX}/share/Geant4/data:ro" \
    "${GATE_SIF}" \
    /job/gate_ore_powell.mac \
    > "${OUTPUT_DIR}/simulation.log" 2>&1
then
    echo "ERROR: GATE execution failed." >&2
    echo "Last 80 lines of simulation.log:" >&2
    tail -80 "${OUTPUT_DIR}/simulation.log" >&2
    exit 1
fi

[[ -s "${OUTPUT_DIR}/gate_ore_powell.root" ]] \
    || fail "GATE ROOT output was not created"

python "${CONVERTER}" \
    "${OUTPUT_DIR}/gate_ore_powell.root" \
    "${OUTPUT_DIR}/annihilation_gammas.csv" \
    --summary "${OUTPUT_DIR}/conversion_summary.json"

python "${REPO_ROOT}/validate_ore_powell_reference.py" \
    "${OUTPUT_DIR}/annihilation_gammas.csv" \
    --bins 50 \
    --subdivisions 12 \
    --minimum-expected-count 5 \
    --output-prefix "${OUTPUT_DIR}/ore_powell_reference" \
    > "${OUTPUT_DIR}/ore_powell_reference.log" 2>&1

python "${REPO_ROOT}/validate_three_gamma_geometry.py" \
    "${OUTPUT_DIR}/annihilation_gammas.csv" \
    --output-prefix "${OUTPUT_DIR}/three_gamma_geometry" \
    > "${OUTPUT_DIR}/three_gamma_geometry.log" 2>&1

python - "${OUTPUT_DIR}" <<'PY'
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])

with (root / "conversion_summary.json").open() as handle:
    conversion = json.load(handle)

with (root / "ore_powell_reference_summary.json").open() as handle:
    analytic = json.load(handle)

with (root / "three_gamma_geometry_summary.json").open() as handle:
    geometry = json.load(handle)

statuses = {
    "conversion": conversion.get("status"),
    "analytic": analytic.get("status"),
    "geometry": geometry.get("status"),
}

if any(value != "PASS" for value in statuses.values()):
    raise SystemExit(f"Benchmark failure: {statuses}")

manifest = {
    "status": "PASS",
    "framework": "GATE",
    "gate_version": "9.4.1",
    "geant4_version": "11.3.0",
    "physics_backend": "GATE Extended oPs",
    "events": conversion["events_written"],
    "photons": conversion["photons_written"],
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
    "photon_phi_z_score": geometry["photon_phi"]["z_score"],
    "plane_normal_cos_theta_z_score": (
        geometry["plane_normal_cos_theta"]["z_score"]
    ),
    "plane_normal_phi_z_score": (
        geometry["plane_normal_phi"]["z_score"]
    ),
}

with (root / "benchmark_manifest.json").open("w") as handle:
    json.dump(manifest, handle, indent=2, sort_keys=True)
    handle.write("\n")

print()
print("=== GATE ORE-POWELL BENCHMARK PASS ===")
print(f"Events                 : {manifest['events']}")
print(f"Photons                : {manifest['photons']}")
print(f"Chi-square             : {manifest['chi_square']:.6f}")
print(f"Degrees of freedom     : {manifest['degrees_of_freedom']}")
print(
    f"Chi-square z score     : "
    f"{manifest['chi_square_z_score']:.6f}"
)
print(
    f"Maximum marginal CDF  : "
    f"{manifest['maximum_marginal_cdf_difference']:.6e}"
)
print(
    f"Maximum axis difference: "
    f"{manifest['maximum_cartesian_axis_mean_difference']:.6e}"
)
print("Status                 : PASS")
PY
