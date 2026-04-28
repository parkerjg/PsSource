# PsSource
Explicit positronium source modeling for Geant4 PET pipelines with controlled 2γ/3γ generation, delay modeling, truth capture, and validation.

## Overview
PsSource provides a simple, modular framework for explicit positronium (Ps) source generation inside Geant4 PET workflows. The code is designed for **methods development**, **pipeline validation**, and **controlled source-side studies** in which the user needs direct control over annihilation topology, timing, and event-level truth.

The package supports:

- direct annihilation, para-positronium, and ortho-positronium branch control
- explicit routing of ortho-positronium events to **2γ** or **3γ** decay
- exponential or fixed delay models
- optional prompt-photon emission
- optional positron-range displacement
- event-level truth capture for downstream validation
- simple validation and compatibility testing scripts

This repository is intended as a **code methods resource** for PET researchers who want to insert explicit positronium modeling into existing Geant4-based simulation pipelines without rewriting their full framework.

---

## Why PsSource?
Standard Geant4 workflows can model positron annihilation through native physics processes. That is appropriate for many problems. PsSource addresses a different need:

- controlled generation of pure **2γ**, pure **3γ**, and mixed datasets
- direct source-side specification of branch fractions and delays
- transparent event-level truth for debugging and validation
- reproducible test harnesses for downstream compatibility checks

The goal is not to provide a finished clinical reconstruction platform. The goal is to provide a **practical source-modeling layer** that PET groups can use, inspect, modify, and integrate.

---

## Core capabilities

### Explicit positronium source model
The explicit source model allows user-defined control of:

- `f_direct` : direct annihilation fraction
- `f_pps` : para-positronium fraction
- `f_ops` : ortho-positronium fraction
- `ortho_3g_fraction` : fraction of o-Ps routed to explicit 3γ decay

### Timing control
Non-direct branches can be sampled with:

- exponential delays
- fixed delays

Separate lifetimes can be assigned for para-Ps and ortho-Ps branches.

### Optional source terms
PsSource also supports:

- optional prompt gamma emission
- optional positron-range displacement
- explicit annihilation-vertex construction
- truth metadata retention for validation

### Validation support
Validation workflows are included for:

- branching-fraction sweeps
- lifetime sweeps
- timing benchmarks
- downstream branch-consistency tests

---

## Main software components

### `PositroniumProvider`
Generates event-level annihilation specifications, including:

- branch assignment
- annihilation mode
- delay
- source and annihilation position
- optional prompt gamma
- optional positron range
- 2γ / 3γ photon topology

### `PositroniumGenerator`
Translates provider output into Geant4 primary vertices and particles.

### `TimedEventModel`
Defines structured event and vertex specifications used by the explicit source path.

### `PositroniumTruthInfo`
Stores event-level truth metadata for downstream validation and analysis.

---

## Front-end executables

Typical front ends include:

- `ps_main` — configurable source-generation driver
- `ps_timing` — truth-capture, timing, and validation driver
- `ps_pointsource` — frozen downstream compatibility / point-source test harness

Depending on how you organize your local build, these may be built from the corresponding C++ drivers in the repository.

---

## Build

PsSource expects a working Geant4 installation.

A typical standalone build uses:

- Geant4
- a C++17 compiler
- `geant4-config` available in the active environment

Example:

```bash
c++ -O2 -std=c++17 $(geant4-config --cflags) \
    main.cc PositroniumGenerator.cc PositroniumProvider.cc \
    -o ps_main \
    $(geant4-config --libs)

c++ -O2 -std=c++17 $(geant4-config --cflags) \
    main_timing.cc PositroniumGenerator.cc PositroniumProvider.cc \
    -o ps_timing \
    $(geant4-config --libs)
````

If you use the included helper script:

```bash
bash build_smoke.sh
```

That script can also be used for a quick end-to-end smoke test.

---

## Quick start

### Native Geant4 reference run

```bash
./ps_main --preset native_reference --beam-on 1000
```

### Explicit pure 2γ run

```bash
./ps_main \
  --generation-mode explicit \
  --f-direct 0 \
  --f-pps 0 \
  --f-ops 1 \
  --ortho-3g-fraction 0.0 \
  --prompt off \
  --beam-on 1000
```

### Explicit pure 3γ run

```bash
./ps_main \
  --generation-mode explicit \
  --f-direct 0 \
  --f-pps 0 \
  --f-ops 1 \
  --ortho-3g-fraction 1.0 \
  --prompt off \
  --beam-on 1000
```

### Explicit mixed run

```bash
./ps_main \
  --generation-mode explicit \
  --f-direct 0.3 \
  --f-pps 0.2 \
  --f-ops 0.5 \
  --ortho-3g-fraction 1.0 \
  --prompt off \
  --beam-on 1000
```

---

## Timing / truth validation

Example:

```bash
./ps_timing \
  --generation-mode explicit \
  --f-direct 0 \
  --f-pps 0 \
  --f-ops 1 \
  --ortho-3g-fraction 1.0 \
  --tau-ops-ns 3.0 \
  --prompt off \
  --beam-on 10000
```

Typical outputs include:

* `annihilation_summary.csv`
* `annihilation_gammas.csv`
* detector hit CSVs
* `run_config.json`

Validation helper:

```bash
python validate_positronium_outputs.py \
  --summary annihilation_summary.csv \
  --gammas annihilation_gammas.csv
```

This reports:

* observed 2γ / 3γ fractions
* mean delays
* energy-sum consistency
* momentum-closure consistency

---

## Reproducibility and scripted studies

The repository includes scripts for structured replicate campaigns and paper-style summaries, including:

* branching-fraction sweeps
* lifetime sweeps
* timing matrices
* point-source downstream compatibility tests
* aggregate post-processing for summary tables / plots

Examples:

```bash
bash run_timing_matrix.sh
bash run_pointsource_matrix.sh
bash run_pointsource_replicates.sh
python aggregate_timing.py
python aggregate_pointsource_results.py
python aggregate_errorbars.py
```

---

## Intended use

PsSource is intended for:

* PET simulation groups developing positronium-capable pipelines
* source-model validation studies
* controlled algorithm testing with known event truth
* methods papers and software reproducibility studies
* integration into existing Geant4 PET workflows

PsSource is **not** intended to claim full detector realism by itself. It is a source-modeling and validation framework.

---

## Manuscript context

This repository accompanies a **code methods paper** focused on explicit positronium source modeling in Geant4 PET pipelines. The downstream reconstruction examples are included only to demonstrate **branch-consistent handling of explicit 2γ, 3γ, and mixed datasets**. They are not presented as a production imaging framework.

---

## Citation

If you use PsSource in published work, please cite the associated manuscript once available.

> Parker JG. *Explicit positronium source modeling for Geant4 PET pipelines: controlled 2-gamma and 3-gamma generation and validation*, arXiv:2604.21173 [physics.med-ph]. https://arxiv.org/abs/2604.21173

---

## License

* MIT

---

## Contact

Jason G. Parker, PhD
parkerjg@iu.edu
Department of Radiology and Imaging Sciences
Indiana University School of Medicine

---
