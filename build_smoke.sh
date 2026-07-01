#!/usr/bin/env bash
set -euo pipefail

unset GLIBC_TUNABLES
hash -r

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

assert_file_nonempty() {
    local f="$1"
    [[ -s "$f" ]] || fail "Expected non-empty file: $f"
}

summarize_modes() {
    local f="$1"
    awk -F, 'NR>1 {count[$9]++} END {for (k in count) print count[k], k}' "$f" | sort -k2,2n
}

summarize_hit_creators() {
    local f="$1"
    awk -F, 'NR>1 && NF >= 5 {count[$5]++} END {for (k in count) print count[k], k}' "$f" | sort -k2,2
}

count_hits_matching() {
    local f="$1"
    local awk_cond="$2"
    awk -F, "NR>1 && NF >= 5 && (${awk_cond}) {c++} END {print c+0}" "$f"
}

summarize_both_hit_creators() {
    local plus="$1"
    local minus="$2"
    awk -F, '
        FNR>1 && NF >= 5 {count[$5]++}
        END {for (k in count) print count[k], k}
    ' "$plus" "$minus" | sort -k2,2
}

if ! command -v geant4-config >/dev/null 2>&1; then
    fail "geant4-config not found in PATH. Activate your Geant4 conda env first."
fi

[[ -n "${CONDA_PREFIX:-}" ]] || fail "CONDA_PREFIX is not set. Activate your Geant4 conda env first."

if [[ -x "$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-c++" ]]; then
    CXX="$CONDA_PREFIX/bin/x86_64-conda-linux-gnu-c++"
elif [[ -x "$CONDA_PREFIX/bin/x86_64-conda_cos6-linux-gnu-c++" ]]; then
    CXX="$CONDA_PREFIX/bin/x86_64-conda_cos6-linux-gnu-c++"
elif [[ -x "$CONDA_PREFIX/bin/x86_64-conda_cos7-linux-gnu-c++" ]]; then
    CXX="$CONDA_PREFIX/bin/x86_64-conda_cos7-linux-gnu-c++"
else
    CXX="${CXX:-c++}"
fi

CXXFLAGS=(-O2 -std=c++17)

G4_CONFIG="$(command -v geant4-config)"
G4_VERSION="$($G4_CONFIG --version)"
G4_CFLAGS_STR="$($G4_CONFIG --cflags)"
G4_LIBS_STR="$($G4_CONFIG --libs)"
RPATH_FLAG="-Wl,-rpath,$CONDA_PREFIX/lib"

export LD_LIBRARY_PATH="$CONDA_PREFIX/lib:${LD_LIBRARY_PATH:-}"

echo "-------------------------------------------------------"
echo "Compiler       : $CXX"
echo "Geant4 config  : $G4_CONFIG"
echo "Geant4 version : $G4_VERSION"
echo "Build mode     : standalone Geant4"
echo "-------------------------------------------------------"

echo
echo "[1/5] Building ps_main ..."
# shellcheck disable=SC2086
"$CXX" "${CXXFLAGS[@]}" $G4_CFLAGS_STR \
    main.cc PositroniumGenerator.cc PositroniumProvider.cc FixedParameterizedPsModel.cc OrePowellPsModel.cc ConfigurablePsModel.cc \
    -o ps_main \
    $RPATH_FLAG \
    $G4_LIBS_STR

echo
echo "[2/5] Building ps_timing ..."
# shellcheck disable=SC2086
"$CXX" "${CXXFLAGS[@]}" $G4_CFLAGS_STR \
    main_timing.cc PositroniumGenerator.cc PositroniumProvider.cc FixedParameterizedPsModel.cc OrePowellPsModel.cc ConfigurablePsModel.cc \
    -o ps_timing \
    $RPATH_FLAG \
    $G4_LIBS_STR

echo
echo "[3/5] Smoke-checking ps_main front end ..."
./ps_main --generation-mode native   --beam-on 5 --prompt on  >/dev/null 2>&1
./ps_main --generation-mode explicit --beam-on 5 --prompt off >/dev/null 2>&1
echo "ps_main ran successfully in both native and explicit modes."

echo
echo "[4/5] Running native Geant4 timing smoke test ..."
rm -f hits.csv hits_plus.csv hits_minus.csv annihilation_summary.csv annihilation_gammas.csv native_smoke.log explicit_smoke.log

./ps_timing \
    --generation-mode native \
    --beam-on 1000 \
    --at-rest-model OrePowellPolar \
    --world-material G4_AIR \
    --orto-ps-fraction 0.50 \
    --prompt on \
    > native_smoke.log 2>&1

assert_file_nonempty hits_plus.csv
assert_file_nonempty hits_minus.csv
assert_file_nonempty annihilation_summary.csv
assert_file_nonempty annihilation_gammas.csv

native_annihil_hits_plus=$(count_hits_matching hits_plus.csv '$5 == "annihil"')
native_annihil_hits_minus=$(count_hits_matching hits_minus.csv '$5 == "annihil"')
native_annihil_hits=$(( native_annihil_hits_plus + native_annihil_hits_minus ))
(( native_annihil_hits > 0 )) || fail "Native smoke test did not record any creator_process=annihil hits."

native_mode_rows=$(
    awk -F, 'NR>1 && ($9 == 2 || $9 == 3) {c++} END {print c+0}' annihilation_summary.csv
)
(( native_mode_rows > 0 )) || fail "Native smoke test did not record any 2g/3g annihilation modes."

echo "Native hits_plus.csv preview:"
head hits_plus.csv
echo
echo "Native hits_minus.csv preview:"
head hits_minus.csv
echo
echo "Native annihilation modes:"
summarize_modes annihilation_summary.csv
echo
echo "Native hit creator processes (combined):"
summarize_both_hit_creators hits_plus.csv hits_minus.csv
echo
echo "Native hit file line counts:"
wc -l hits_plus.csv hits_minus.csv

echo
echo "[5/5] Running explicit provider timing smoke test ..."
rm -f hits.csv hits_plus.csv hits_minus.csv annihilation_summary.csv annihilation_gammas.csv

./ps_timing \
    --generation-mode explicit \
    --beam-on 1000 \
    --f-direct 0 \
    --f-pps 0 \
    --f-ops 1 \
    --ortho-3g-fraction 1.0 \
    --tau-ops-ns 3.0 \
    --prompt off \
    --positron-range off \
    > explicit_smoke.log 2>&1

assert_file_nonempty hits_plus.csv
assert_file_nonempty hits_minus.csv
assert_file_nonempty annihilation_summary.csv
assert_file_nonempty annihilation_gammas.csv

explicit_non3=$(
    awk -F, 'NR>1 && $9 != 3 {c++} END {print c+0}' annihilation_summary.csv
)
(( explicit_non3 == 0 )) || fail "Explicit smoke test produced non-3g annihilation modes."

explicit_primary_hits_plus=$(count_hits_matching hits_plus.csv '$4 == 22 && $5 == "PRIMARY"')
explicit_primary_hits_minus=$(count_hits_matching hits_minus.csv '$4 == 22 && $5 == "PRIMARY"')
explicit_primary_hits=$(( explicit_primary_hits_plus + explicit_primary_hits_minus ))
(( explicit_primary_hits > 0 )) || fail "Explicit smoke test did not record any PRIMARY gamma detector hits."

explicit_annihil_hits_plus=$(count_hits_matching hits_plus.csv '$5 == "annihil"')
explicit_annihil_hits_minus=$(count_hits_matching hits_minus.csv '$5 == "annihil"')
explicit_annihil_hits=$(( explicit_annihil_hits_plus + explicit_annihil_hits_minus ))
(( explicit_annihil_hits == 0 )) || fail "Explicit smoke test produced native annihil detector hits."

explicit_unique_times=$(
    awk -F, 'NR>1 {print $11}' annihilation_summary.csv | sort -u | wc -l | tr -d ' '
)
(( explicit_unique_times > 10 )) || fail "Explicit smoke test did not show a distributed annihilation_time_ns."

echo "Explicit hits_plus.csv preview:"
head hits_plus.csv
echo
echo "Explicit hits_minus.csv preview:"
head hits_minus.csv
echo
echo "Explicit annihilation modes:"
summarize_modes annihilation_summary.csv
echo
echo "Explicit hit creator processes (combined):"
summarize_both_hit_creators hits_plus.csv hits_minus.csv
echo
echo "Explicit annihilation_gammas.csv preview:"
head annihilation_gammas.csv
echo
echo "Explicit hit file line counts:"
wc -l hits_plus.csv hits_minus.csv

echo
echo "Smoke test completed successfully."
echo "  - native Geant4 mode: PASS"
echo "  - explicit provider mode: PASS"
