#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")"
    pwd
)"

RUN_ROOT="${ROOT_DIR}/regression_runs"
EVENT_COUNT="${1:-5000}"

if [[ ! "${EVENT_COUNT}" =~ ^[1-9][0-9]*$ ]]; then
    echo "ERROR: event count must be a positive integer."
    exit 1
fi

PS_TIMING="${ROOT_DIR}/ps_timing"
OUTPUT_VALIDATOR="${ROOT_DIR}/validate_positronium_outputs.py"
ISOTROPY_VALIDATOR="${ROOT_DIR}/validate_isotropy.py"
MODEL_COMPARISON="${ROOT_DIR}/compare_three_gamma_models.py"

fail()
{
    echo
    echo "REGRESSION FAILURE: $*" >&2
    exit 1
}

require_file()
{
    local path="$1"

    [[ -f "${path}" ]] ||
        fail "Required file not found: ${path}"
}

check_csv_header()
{
    local summary_csv="$1"
    local header

    header="$(head -1 "${summary_csv}")"

    [[ "${header}" == *"physics_model_name,physics_model_version,physics_validation_status"* ]] ||
        fail "Provenance columns missing from ${summary_csv}"
}

check_event_counts()
{
    local summary_csv="$1"
    local expected_count="$2"

    local total_events
    local mode3_events
    local three_gamma_events

    total_events="$(
        awk -F, 'NR > 1 {count++} END {print count + 0}' \
            "${summary_csv}"
    )"

    mode3_events="$(
        awk -F, '
            NR == 1 {
                for (i = 1; i <= NF; ++i) {
                    if ($i == "annihilation_mode") {
                        column = i
                    }
                }
                next
            }

            $column == 3 {
                count++
            }

            END {
                print count + 0
            }
        ' "${summary_csv}"
    )"

    three_gamma_events="$(
        awk -F, '
            NR == 1 {
                for (i = 1; i <= NF; ++i) {
                    if ($i == "n_annihilation_gammas") {
                        column = i
                    }
                }
                next
            }

            $column == 3 {
                count++
            }

            END {
                print count + 0
            }
        ' "${summary_csv}"
    )"

    [[ "${total_events}" -eq "${expected_count}" ]] ||
        fail "Expected ${expected_count} events, found ${total_events}"

    [[ "${mode3_events}" -eq "${expected_count}" ]] ||
        fail "Expected ${expected_count} mode-3 events, found ${mode3_events}"

    [[ "${three_gamma_events}" -eq "${expected_count}" ]] ||
        fail "Expected ${expected_count} three-photon events, found ${three_gamma_events}"
}

check_gamma_rows()
{
    local gamma_csv="$1"
    local expected_events="$2"

    local expected_rows
    local actual_rows

    expected_rows=$((3 * expected_events))

    actual_rows="$(
        awk 'NR > 1 {count++} END {print count + 0}' \
            "${gamma_csv}"
    )"

    [[ "${actual_rows}" -eq "${expected_rows}" ]] ||
        fail "Expected ${expected_rows} photon rows, found ${actual_rows}"
}

check_run_config()
{
    local run_config="$1"
    local expected_cli_model="$2"

    grep -Fq \
        "\"three_gamma_model\": \"${expected_cli_model}\"" \
        "${run_config}" ||
        fail "Incorrect three_gamma_model in ${run_config}"
}

check_event_provenance()
{
    local summary_csv="$1"
    local expected_name="$2"
    local expected_version="$3"
    local expected_status="$4"

    awk -F, \
        -v expected_name="${expected_name}" \
        -v expected_version="${expected_version}" \
        -v expected_status="${expected_status}" '
        NR == 1 {
            for (i = 1; i <= NF; ++i) {
                if ($i == "physics_model_name") {
                    name_column = i
                }
                if ($i == "physics_model_version") {
                    version_column = i
                }
                if ($i == "physics_validation_status") {
                    status_column = i
                }
            }

            if (!name_column || !version_column || !status_column) {
                exit 2
            }

            next
        }

        {
            if ($name_column != expected_name ||
                $version_column != expected_version ||
                $status_column != expected_status) {

                print "Unexpected provenance at event row " NR ": " \
                      $name_column "," \
                      $version_column "," \
                      $status_column > "/dev/stderr"

                exit 1
            }
        }
    ' "${summary_csv}" ||
        fail "Event provenance check failed for ${summary_csv}"
}

run_backend()
{
    local label="$1"
    local cli_model="$2"
    local expected_name="$3"
    local expected_version="$4"
    local expected_status="$5"

    local run_dir="${RUN_ROOT}/${label}"

    echo
    echo "============================================================"
    echo "Running backend: ${label}"
    echo "CLI model      : ${cli_model}"
    echo "Events         : ${EVENT_COUNT}"
    echo "============================================================"

    rm -rf "${run_dir}"
    mkdir -p "${run_dir}"

    (
        cd "${run_dir}"

        "${PS_TIMING}" \
            --generation-mode explicit \
            --beam-on "${EVENT_COUNT}" \
            --f-direct 0 \
            --f-pps 0 \
            --f-ops 1 \
            --ortho-3g-fraction 1 \
            --three-gamma-model "${cli_model}" \
            --tau-ops-ns 3 \
            --prompt off \
            > run.log 2>&1

        python "${OUTPUT_VALIDATOR}" \
            > validation.log 2>&1

        python "${ISOTROPY_VALIDATOR}" \
            > isotropy.log 2>&1
    )

    require_file "${run_dir}/run_config.json"
    require_file "${run_dir}/annihilation_summary.csv"
    require_file "${run_dir}/annihilation_gammas.csv"
    require_file "${run_dir}/validation.log"
    require_file "${run_dir}/isotropy.log"

    check_csv_header \
        "${run_dir}/annihilation_summary.csv"

    check_event_counts \
        "${run_dir}/annihilation_summary.csv" \
        "${EVENT_COUNT}"

    check_gamma_rows \
        "${run_dir}/annihilation_gammas.csv" \
        "${EVENT_COUNT}"

    check_run_config \
        "${run_dir}/run_config.json" \
        "${cli_model}"

    check_event_provenance \
        "${run_dir}/annihilation_summary.csv" \
        "${expected_name}" \
        "${expected_version}" \
        "${expected_status}"

    grep -Fq "PASS" "${run_dir}/isotropy.log" ||
        fail "Isotropy validator did not report PASS for ${label}"

    echo "${label}: PASS"
}

test_invalid_model()
{
    local invalid_dir="${RUN_ROOT}/invalid_model_test"

    echo
    echo "Testing invalid CLI model handling..."

    rm -rf "${invalid_dir}"
    mkdir -p "${invalid_dir}"

    if (
        cd "${invalid_dir}"

        "${PS_TIMING}" \
            --generation-mode explicit \
            --beam-on 1 \
            --three-gamma-model definitely-not-a-model \
            > run.log 2>&1
    ); then
        fail "Invalid model name was accepted."
    fi

    grep -Fq \
        "Invalid --three-gamma-model" \
        "${invalid_dir}/run.log" ||
        fail "Invalid model failed, but expected error message was absent."

    echo "invalid model handling: PASS"
}

run_model_comparison()
{
    echo
    echo "Running three-backend distribution comparison..."

    (
        cd "${RUN_ROOT}"

        python "${MODEL_COMPARISON}" \
            --input \
            approximate:approximate/annihilation_gammas.csv \
            --input \
            ore_powell:ore_powell/annihilation_gammas.csv \
            --input \
            polarized:ore_powell_polarized/annihilation_gammas.csv \
            --bins 100 \
            --output-prefix regression_comparison \
            --no-plots \
            > model_comparison.log 2>&1
    )

    require_file \
        "${RUN_ROOT}/regression_comparison_summary.csv"

    require_file \
        "${RUN_ROOT}/regression_comparison_pairwise.csv"

    echo "model comparison: PASS"
}

echo "=== PsSource Three-Gamma Regression Suite ==="
echo "Root directory: ${ROOT_DIR}"
echo "Run directory : ${RUN_ROOT}"
echo "Event count   : ${EVENT_COUNT}"

require_file "${PS_TIMING}"
require_file "${OUTPUT_VALIDATOR}"
require_file "${ISOTROPY_VALIDATOR}"
require_file "${MODEL_COMPARISON}"

rm -rf "${RUN_ROOT}"
mkdir -p "${RUN_ROOT}"

run_backend \
    "approximate" \
    "approximate" \
    "ConfigurablePsModel/ApproximatePhaseSpace" \
    "1.0" \
    "approximate-controlled-source-model"

run_backend \
    "ore_powell" \
    "ore-powell" \
    "ConfigurablePsModel/Geant4OrePowell" \
    "Geant4-11.3.2" \
    "geant4-native-ore-powell"

run_backend \
    "ore_powell_polarized" \
    "ore-powell-polarized" \
    "ConfigurablePsModel/Geant4PolarizedOrePowell" \
    "Geant4-11.3.2" \
    "geant4-native-ore-powell"

test_invalid_model
run_model_comparison

echo
echo "============================================================"
echo "THREE-GAMMA REGRESSION SUITE: PASS"
echo "============================================================"
echo
echo "Outputs:"
echo "  ${RUN_ROOT}"
