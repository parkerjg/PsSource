#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")"
    pwd
)"

SOURCE_FILE="${SCRIPT_DIR}/native_geant4_ore_powell.cc"
OUTPUT_FILE="${SCRIPT_DIR}/native_geant4_ore_powell"

if ! command -v geant4-config >/dev/null 2>&1; then
    echo "ERROR: geant4-config was not found." >&2
    exit 1
fi

if [[ -n "${CONDA_PREFIX:-}" ]] &&
   [[ -x "${CONDA_PREFIX}/bin/x86_64-conda-linux-gnu-c++" ]]
then
    CXX="${CONDA_PREFIX}/bin/x86_64-conda-linux-gnu-c++"
else
    CXX="${CXX:-c++}"
fi

G4_CFLAGS="$(geant4-config --cflags)"
G4_LIBS="$(geant4-config --libs)"

RPATH_ARGS=()

if [[ -n "${CONDA_PREFIX:-}" ]]; then
    RPATH_ARGS+=(
        "-Wl,-rpath,${CONDA_PREFIX}/lib"
    )

    export LD_LIBRARY_PATH="${CONDA_PREFIX}/lib:${LD_LIBRARY_PATH:-}"
fi

echo "=== Build native Geant4 reference ==="
echo "Compiler       : ${CXX}"
echo "Geant4 version : $(geant4-config --version)"
echo "Source         : ${SOURCE_FILE}"
echo "Output         : ${OUTPUT_FILE}"

# shellcheck disable=SC2086
"${CXX}" \
    -O2 \
    -std=c++17 \
    -Wall \
    -Wextra \
    -pedantic \
    ${G4_CFLAGS} \
    "${SOURCE_FILE}" \
    -o "${OUTPUT_FILE}" \
    "${RPATH_ARGS[@]}" \
    ${G4_LIBS}

echo "PASS"
