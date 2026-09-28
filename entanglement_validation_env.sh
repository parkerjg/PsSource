# Activate the Geant4 conda environment for the entanglement validation.
#
# Sourced by run_entanglement_validation_br200.sh and the Slurm jobs. It does
# what "conda activate" does for this purpose without requiring conda init:
# put the environment first on PATH and run its activation scripts, which
# set the compiler variables and the Geant4 data paths (G4*DATA).

activate_pssource_env() {
    local env="${PSSOURCE_ENV:-/N/project/MIPHYS/QEPET/env}"

    if [[ ! -x "$env/bin/geant4-config" ]]; then
        echo "ERROR: geant4-config not found in $env/bin. Set PSSOURCE_ENV to your Geant4 conda environment." >&2
        return 1
    fi

    # Conda activation scripts are not written for "set -eu".
    local restore_flags="$-"
    set +eu

    unset GLIBC_TUNABLES
    export CONDA_PREFIX="$env"
    export PATH="$env/bin:$PATH"

    local script
    for script in "$env"/etc/conda/activate.d/*.sh; do
        [[ -e "$script" ]] || continue
        # shellcheck disable=SC1090
        source "$script"
    done

    [[ "$restore_flags" == *e* ]] && set -e
    [[ "$restore_flags" == *u* ]] && set -u

    hash -r

    # Prefer the conda C++ compiler, as build_smoke.sh does.
    if [[ -z "${CXX:-}" ]]; then
        if [[ -x "$env/bin/x86_64-conda-linux-gnu-c++" ]]; then
            export CXX="$env/bin/x86_64-conda-linux-gnu-c++"
        else
            export CXX=c++
        fi
    fi
}
