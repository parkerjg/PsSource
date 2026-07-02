#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(
    cd "$(dirname "${BASH_SOURCE[0]}")"
    pwd
)"

OUTPUT_DIR="${OUTPUT_DIR:-${REPO_ROOT}/cross_framework_ore_powell_run}"

PSSOURCE_CSV="${PSSOURCE_CSV:-${REPO_ROOT}/ore_powell_benchmark_run/annihilation_gammas.csv}"
NATIVE_GEANT4_CSV="${NATIVE_GEANT4_CSV:-${REPO_ROOT}/native_geant4_ore_powell_run/annihilation_gammas.csv}"
GATE_CSV="${GATE_CSV:-${REPO_ROOT}/gate_ore_powell_run/annihilation_gammas.csv}"

BINS="${BINS:-80}"

MAX_UNORDERED_KS="${MAX_UNORDERED_KS:-0.01}"
MAX_ORDERED_KS="${MAX_ORDERED_KS:-0.01}"
MAX_HISTOGRAM_L1="${MAX_HISTOGRAM_L1:-0.05}"

COMPARISON_SCRIPT="${REPO_ROOT}/compare_three_gamma_models.py"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

require_file() {
    [[ -f "$1" ]] || fail "Required file not found: $1"
}

require_file "${COMPARISON_SCRIPT}"
require_file "${PSSOURCE_CSV}"
require_file "${NATIVE_GEANT4_CSV}"
require_file "${GATE_CSV}"

if [[ -e "${OUTPUT_DIR}" ]]; then
    fail "Output directory already exists: ${OUTPUT_DIR}"
fi

mkdir -p "${OUTPUT_DIR}"

OUTPUT_PREFIX="${OUTPUT_DIR}/cross_framework"

echo "=== Cross-framework Ore-Powell comparison ==="
echo "PsSource CSV       : ${PSSOURCE_CSV}"
echo "Native Geant4 CSV  : ${NATIVE_GEANT4_CSV}"
echo "GATE CSV           : ${GATE_CSV}"
echo "Output directory   : ${OUTPUT_DIR}"
echo "Histogram bins     : ${BINS}"
echo
echo "Acceptance thresholds:"
echo "  unordered KS     <= ${MAX_UNORDERED_KS}"
echo "  ordered KS       <= ${MAX_ORDERED_KS}"
echo "  histogram L1     <= ${MAX_HISTOGRAM_L1}"
echo

{
    echo "timestamp_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "hostname=$(hostname)"
    echo "repository=${REPO_ROOT}"
    echo "git_commit=$(git -C "${REPO_ROOT}" rev-parse HEAD)"
    echo "python=$(command -v python)"
    python --version
    echo "pssource_csv=${PSSOURCE_CSV}"
    echo "native_geant4_csv=${NATIVE_GEANT4_CSV}"
    echo "gate_csv=${GATE_CSV}"
    echo "bins=${BINS}"
    echo "maximum_unordered_ks=${MAX_UNORDERED_KS}"
    echo "maximum_ordered_ks=${MAX_ORDERED_KS}"
    echo "maximum_histogram_l1=${MAX_HISTOGRAM_L1}"
} > "${OUTPUT_DIR}/environment.txt" 2>&1

git -C "${REPO_ROOT}" status --short \
    > "${OUTPUT_DIR}/git_status.txt"

cat > "${OUTPUT_DIR}/command.sh" <<EOF
python "${COMPARISON_SCRIPT}" \\
    --input "pssource:${PSSOURCE_CSV}" \\
    --input "native_geant4:${NATIVE_GEANT4_CSV}" \\
    --input "gate:${GATE_CSV}" \\
    --bins "${BINS}" \\
    --output-prefix "${OUTPUT_PREFIX}"
EOF

chmod +x "${OUTPUT_DIR}/command.sh"

echo "[1/3] Running three-framework comparison"

python "${COMPARISON_SCRIPT}" \
    --input "pssource:${PSSOURCE_CSV}" \
    --input "native_geant4:${NATIVE_GEANT4_CSV}" \
    --input "gate:${GATE_CSV}" \
    --bins "${BINS}" \
    --output-prefix "${OUTPUT_PREFIX}" \
    > "${OUTPUT_DIR}/comparison.log" 2>&1

PAIRWISE_CSV="${OUTPUT_PREFIX}_pairwise.csv"
SUMMARY_CSV="${OUTPUT_PREFIX}_summary.csv"

require_file "${PAIRWISE_CSV}"
require_file "${SUMMARY_CSV}"

echo "[2/3] Checking pairwise acceptance thresholds"

python - \
    "${PAIRWISE_CSV}" \
    "${SUMMARY_CSV}" \
    "${OUTPUT_DIR}" \
    "${MAX_UNORDERED_KS}" \
    "${MAX_ORDERED_KS}" \
    "${MAX_HISTOGRAM_L1}" <<'PY'
import csv
import json
import math
import sys
from pathlib import Path

pairwise_path = Path(sys.argv[1])
summary_path = Path(sys.argv[2])
output_dir = Path(sys.argv[3])

maximum_unordered_ks = float(sys.argv[4])
maximum_ordered_ks = float(sys.argv[5])
maximum_histogram_l1 = float(sys.argv[6])


def read_csv(path):
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


pairwise_rows = read_csv(pairwise_path)
summary_rows = read_csv(summary_path)

expected_labels = {
    "pssource",
    "native_geant4",
    "gate",
}

found_labels = {
    row["label"]
    for row in summary_rows
}

if found_labels != expected_labels:
    raise SystemExit(
        "Unexpected model labels. "
        f"Expected {sorted(expected_labels)}, "
        f"found {sorted(found_labels)}"
    )

expected_pairs = {
    frozenset(("pssource", "native_geant4")),
    frozenset(("pssource", "gate")),
    frozenset(("native_geant4", "gate")),
}

found_pairs = {
    frozenset((row["model_a"], row["model_b"]))
    for row in pairwise_rows
}

if found_pairs != expected_pairs:
    raise SystemExit(
        "Unexpected pairwise comparison set. "
        f"Found: {sorted(map(sorted, found_pairs))}"
    )

failures = []
pairwise_results = []

for row in pairwise_rows:
    unordered_ks = float(row["unordered_marginal_ks"])
    histogram_l1 = float(row["unordered_histogram_l1"])
    ordered_low_ks = float(row["ordered_low_ks"])
    ordered_mid_ks = float(row["ordered_mid_ks"])
    ordered_high_ks = float(row["ordered_high_ks"])

    largest_ordered_ks = max(
        ordered_low_ks,
        ordered_mid_ks,
        ordered_high_ks,
    )

    comparison = (
        f"{row['model_a']} vs {row['model_b']}"
    )

    if unordered_ks > maximum_unordered_ks:
        failures.append(
            f"{comparison}: unordered marginal KS "
            f"{unordered_ks:.8f} exceeds "
            f"{maximum_unordered_ks:.8f}"
        )

    if largest_ordered_ks > maximum_ordered_ks:
        failures.append(
            f"{comparison}: largest ordered-energy KS "
            f"{largest_ordered_ks:.8f} exceeds "
            f"{maximum_ordered_ks:.8f}"
        )

    if histogram_l1 > maximum_histogram_l1:
        failures.append(
            f"{comparison}: unordered histogram L1 "
            f"{histogram_l1:.8f} exceeds "
            f"{maximum_histogram_l1:.8f}"
        )

    pairwise_results.append(
        {
            "model_a": row["model_a"],
            "model_b": row["model_b"],
            "comparison": comparison,
            "unordered_marginal_ks": unordered_ks,
            "unordered_histogram_l1": histogram_l1,
            "ordered_low_ks": ordered_low_ks,
            "ordered_mid_ks": ordered_mid_ks,
            "ordered_high_ks": ordered_high_ks,
            "largest_ordered_energy_ks": (
                largest_ordered_ks
            ),
        }
    )

models = {}

for row in summary_rows:
    label = row["label"]

    models[label] = {
        "input_path": row["input_path"],
        "event_count": int(row["event_count"]),
        "photon_count": int(row["photon_count"]),
        "mean_x": float(row["mean_x"]),
        "std_x": float(row["std_x"]),
        "minimum_x": float(row["min_x"]),
        "q05_x": float(row["q05_x"]),
        "median_x": float(row["median_x"]),
        "q95_x": float(row["q95_x"]),
        "maximum_x": float(row["max_x"]),
        "mean_low_x": float(row["mean_low_x"]),
        "mean_mid_x": float(row["mean_mid_x"]),
        "mean_high_x": float(row["mean_high_x"]),
        "maximum_absolute_energy_error_MeV": float(
            row["max_abs_energy_error_MeV"]
        ),
        "support_violation_count": int(
            row["support_violation_count"]
        ),
    }

for label, model in models.items():
    if model["event_count"] <= 0:
        failures.append(
            f"{label}: no events were analyzed"
        )

    if model["photon_count"] != 3 * model["event_count"]:
        failures.append(
            f"{label}: photon count is not three times "
            "the event count"
        )

    if model["support_violation_count"] != 0:
        failures.append(
            f"{label}: normalized-energy support violations "
            f"were found"
        )

manifest = {
    "status": "PASS" if not failures else "FAIL",
    "comparison": (
        "Ordinary Ore-Powell three-photon decay across "
        "PsSource, native Geant4, and GATE"
    ),
    "models": models,
    "pairwise_results": pairwise_results,
    "acceptance_thresholds": {
        "maximum_unordered_marginal_ks": (
            maximum_unordered_ks
        ),
        "maximum_ordered_energy_ks": (
            maximum_ordered_ks
        ),
        "maximum_unordered_histogram_l1": (
            maximum_histogram_l1
        ),
    },
    "maximum_observed_values": {
        "unordered_marginal_ks": max(
            row["unordered_marginal_ks"]
            for row in pairwise_results
        ),
        "ordered_energy_ks": max(
            row["largest_ordered_energy_ks"]
            for row in pairwise_results
        ),
        "unordered_histogram_l1": max(
            row["unordered_histogram_l1"]
            for row in pairwise_results
        ),
    },
    "failures": failures,
}

manifest_path = (
    output_dir / "cross_framework_manifest.json"
)

with manifest_path.open("w", encoding="utf-8") as handle:
    json.dump(
        manifest,
        handle,
        indent=2,
        sort_keys=True,
    )
    handle.write("\n")

table_path = (
    output_dir / "manuscript_pairwise_summary.csv"
)

with table_path.open(
    "w",
    newline="",
    encoding="utf-8",
) as handle:
    fieldnames = [
        "comparison",
        "unordered_marginal_ks",
        "largest_ordered_energy_ks",
        "unordered_histogram_l1",
    ]

    writer = csv.DictWriter(
        handle,
        fieldnames=fieldnames,
    )

    writer.writeheader()

    for row in pairwise_results:
        writer.writerow(
            {
                "comparison": row["comparison"],
                "unordered_marginal_ks": (
                    f"{row['unordered_marginal_ks']:.10f}"
                ),
                "largest_ordered_energy_ks": (
                    f"{row['largest_ordered_energy_ks']:.10f}"
                ),
                "unordered_histogram_l1": (
                    f"{row['unordered_histogram_l1']:.10f}"
                ),
            }
        )

print("Pairwise results:")

for row in pairwise_results:
    print(
        f"  {row['comparison']}: "
        f"marginal KS="
        f"{row['unordered_marginal_ks']:.8f}, "
        f"largest ordered KS="
        f"{row['largest_ordered_energy_ks']:.8f}, "
        f"histogram L1="
        f"{row['unordered_histogram_l1']:.8f}"
    )

print()
print(f"Manifest: {manifest_path}")
print(f"Table   : {table_path}")

if failures:
    print()
    print("FAIL")

    for failure in failures:
        print(f"  - {failure}")

    raise SystemExit(1)

print()
print("PASS")
PY

echo "[3/3] Verifying generated artifacts"

require_file "${OUTPUT_DIR}/cross_framework_manifest.json"
require_file "${OUTPUT_DIR}/manuscript_pairwise_summary.csv"
require_file "${OUTPUT_DIR}/comparison.log"

echo
echo "=== CROSS-FRAMEWORK ORE-POWELL COMPARISON PASS ==="

python - \
    "${OUTPUT_DIR}/cross_framework_manifest.json" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    manifest = json.load(handle)

for row in manifest["pairwise_results"]:
    print(
        f"{row['comparison']}: "
        f"marginal KS="
        f"{row['unordered_marginal_ks']:.6f}, "
        f"largest ordered KS="
        f"{row['largest_ordered_energy_ks']:.6f}"
    )

print(
    "Maximum marginal KS     : "
    f"{manifest['maximum_observed_values']['unordered_marginal_ks']:.6f}"
)

print(
    "Maximum ordered-energy KS: "
    f"{manifest['maximum_observed_values']['ordered_energy_ks']:.6f}"
)

print("Status                  : PASS")
PY
