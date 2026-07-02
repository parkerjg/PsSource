# GATE Ore-Powell Reference Benchmark

This directory provides a reproducible external-framework reference for
ordinary orthopositronium three-photon decay using the GATE Extended source.

## Reference environment

- GATE 9.4.1
- Geant4 11.3.0
- Apptainer image derived from `opengatecollaboration/gate:latest`
- The frozen image SHA-256 is recorded in `gate_sif_sha256.txt`

The container image itself is not stored in this repository.

## Benchmark design

The macro creates an orthopositronium source using:

    /gate/source/addSource PsSource Extended
    /gate/source/PsSource/setType oPs

A thin spherical phase-space shell made from `G4_Galactic` surrounds the
source. This records the three untransported primary photons from each decay
while avoiding material interactions.

The converter:

1. selects the newest `PhaseSpace` ROOT cycle;
2. retains outward-moving primary gamma records;
3. requires exactly three photons per event;
4. checks energy and momentum closure;
5. writes the common `annihilation_gammas.csv` schema.

The converted output is evaluated using the same independent analytic
Ore-Powell and geometry/isotropy validators used for PsSource and the
standalone native-Geant4 reference.

## Geant4 data dependency

The container image does not include all Geant4 data libraries. The runner
uses the active conda environment and binds:

    ${CONDA_PREFIX}/share/Geant4/data

into the container at the same absolute path so the inherited Geant4 data
environment variables remain valid.

## Running

Load Apptainer and activate an environment containing the Geant4 data files:

    module load apptainer

    GATE_SIF=/path/to/gate.sif \
    bash reference/gate_ore_powell/run_gate_ore_powell.sh

The default output directory is:

    gate_ore_powell_run/

Generated acquisitions are intentionally excluded from Git.
