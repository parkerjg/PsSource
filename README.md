# PsSource

PsSource is an installable Geant4 physics extension for configurable positronium-aware terminal positron annihilation.

It allows an existing Geant4 application to retain its normal primary source, geometry, positron transport, detector construction, scoring, digitization, analysis, and output infrastructure while replacing only the terminal positron at-rest annihilation process.

## Where PsSource takes over

The host application creates the positron and Geant4 transports it normally.

PsSource does not replace positron creation, radioactive decay, in-flight transport, energy loss, scattering, or range modeling. It takes over only when the transported positron reaches the terminal Geant4 at-rest annihilation stage.

At that handoff, PsSource:

1. captures the transported terminal position, global time, and local Geant4 environment;
2. resolves a `PsEnvironment` from either fixed configuration or a host-provided environment provider;
3. selects direct 2γ, p-Ps 2γ, o-Ps 2γ, or o-Ps 3γ behavior;
4. samples the configured fixed or exponential positronium delay;
5. generates the selected two- or three-photon final state;
6. returns the annihilation photons as ordinary Geant4 secondaries.

The generated photons then undergo standard Geant4 tracking and interact with the host geometry, materials, fields, sensitive detectors, and scoring systems without PsSource-specific downstream handling.

This terminal-state handoff is a computational integration boundary. It does not assert that microscopic positronium formation occurs only after the positron reaches rest.

## Recommended integration path

The recommended public integration path is transport-coupled registration of `PsSourceAnnihilationPhysics` with an existing `G4VModularPhysicsList`.

The host application does **not** need to use:

* `PositroniumGenerator`;
* `PositroniumProvider`;
* `PositroniumTruthInfo`;
* a PsSource event action;
* a PsSource tracking action;
* a PsSource-specific primary generator;
* a PsSource-specific output format.

A minimal integration has the following form:

```cpp
#include "PsSourceAnnihilationPhysics.hh"

#include "G4PhysListFactory.hh"
#include "G4VModularPhysicsList.hh"

G4PhysListFactory factory;

G4VModularPhysicsList* physics =
    factory.GetReferencePhysList("FTFP_BERT");

PsSourceAnnihilationConfig config;

// Configure config.environment, delay behavior,
// and the three-photon model here.

physics->RegisterPhysics(
    new PsSourceAnnihilationPhysics(config)
);

run_manager->SetUserInitialization(physics);
```

When `PsSourceAnnihilationPhysics` is not registered, the application retains its native Geant4 positron-annihilation behavior.

## Supported annihilation behavior

The transport-coupled implementation currently supports:

* direct annihilation to 2γ;
* para-positronium annihilation to 2γ;
* ortho-positronium annihilation to 2γ;
* ortho-positronium annihilation to 3γ;
* fixed delays;
* exponentially sampled delays;
* approximate controlled three-photon phase space;
* native Geant4 Ore–Powell three-photon generation;
* native Geant4 polarized Ore–Powell generation;
* fixed environment configuration;
* host-defined local environment resolution;
* optional annihilation observation and truth capture.

Environmental branch fractions and lifetime parameters are supplied by the application. PsSource does not infer tissue-, material-, or chemistry-specific positronium behavior from Geant4 material composition.

## Legacy source-generator path

The repository retains an explicit source-generator path based on `PositroniumGenerator`, `PositroniumProvider`, `TimedEventSpec`, and `PositroniumTruthInfo`.

That path remains useful for validation, controlled source studies, cross-framework comparisons, manuscript reproducibility, and regression testing. It is not the recommended integration path for an existing Geant4 application.

---

## Public API

PsSource installs exactly seven public headers.

### `PsSourceAnnihilationPhysics.hh`

Defines `PsSourceAnnihilationPhysics`, the Geant4 physics constructor registered with a host `G4VModularPhysicsList`.

This is the primary integration class. It installs the PsSource terminal at-rest annihilation process for positrons.

### `PsSourceAnnihilationConfig.hh`

Defines `PsSourceAnnihilationConfig`, the complete configuration passed to `PsSourceAnnihilationPhysics`.

It contains:

* the fallback fixed `PsEnvironment`;
* an optional environment-provider pointer;
* the fixed or exponential delay mode;
* the fixed delay value;
* three-photon enablement;
* the selected three-photon model;
* an optional annihilation-observer pointer.

The environment-provider and observer pointers are non-owning. The host application must keep the referenced objects alive for as long as the Geant4 run manager may use the registered PsSource physics.

### `PsSourceTypes.hh`

Defines the shared public value types and enumerations:

* `PsClass`;
* `PsEnvironment`;
* `PsSourceDelayMode`;
* `PsSourceThreeGammaModel`.

`PsEnvironment` contains the application-supplied branch fractions, lifetimes, ortho-positronium 2γ/3γ fractions, and medium identifier used for one terminal annihilation.

### `PsTerminalState.hh`

Defines `PsTerminalState`, the transported positron state captured at the terminal handoff.

It includes:

* event and track identifiers;
* source position and time;
* terminal position and global time;
* initial and terminal kinetic energy;
* track length;
* source-to-terminal displacement;
* local material, region, logical-volume, and physical-volume pointers;
* copy number;
* touchable copy-number hierarchy.

The Geant4 pointers in `PsTerminalState` describe the local transport state. A provider should inspect them during `ResolveEnvironment()` and should not assume ownership of them.

### `PsSourceEnvironmentProvider.hh`

Defines the abstract `IPsSourceEnvironmentProvider` interface:

```cpp
class IPsSourceEnvironmentProvider {
public:
    virtual ~IPsSourceEnvironmentProvider() = default;

    virtual PsEnvironment ResolveEnvironment(
        const PsTerminalState& terminal_state
    ) const = 0;
};
```

A host application can implement this interface to select local positronium parameters from the transported terminal state.

Material-, region-, volume-, copy-number-, field-, database-, or phantom-property mapping remains application policy rather than PsSource policy.

### `FixedPsSourceEnvironmentProvider.hh`

Defines `FixedPsSourceEnvironmentProvider`, a convenience provider that returns the same stored `PsEnvironment` for every terminal state.

It is useful when an application prefers the provider interface but does not require spatially varying parameters.

Using this provider is physically equivalent to assigning the same environment directly to `PsSourceAnnihilationConfig::environment`.

### `PsSourceAnnihilationObserver.hh`

Defines:

* `PsSourceAnnihilationRecord`;
* `IPsSourceAnnihilationObserver`.

An optional observer receives one record after PsSource realizes a terminal annihilation. The record contains the realized Ps class, photon multiplicity, terminal time, sampled delay, annihilation time and position, local environment metadata, and physics-model provenance.

An observer is optional. PsSource does not require a PsSource event action, tracking action, truth object, or output format.

---

## Configuring the positronium environment

PsSource requires a `PsEnvironment` for each terminal positron annihilation. The environment may come from:

1. the fixed fallback stored directly in `PsSourceAnnihilationConfig`;
2. `FixedPsSourceEnvironmentProvider`;
3. a host-defined implementation of `IPsSourceEnvironmentProvider`.

The fractions `f_direct`, `f_pps`, and `f_ops` must be non-negative and sum to one. The fractions `ops_2g_fraction` and `ops_3g_fraction` must also be non-negative and sum to one.

### Fixed `PsEnvironment` configuration

For a spatially uniform environment, configure `PsSourceAnnihilationConfig::environment` directly:

```cpp id="ttg3m5"
#include "PsSourceAnnihilationPhysics.hh"

PsSourceAnnihilationConfig config;

config.environment.medium_id = 1;

config.environment.f_direct = 0.30;
config.environment.f_pps = 0.20;
config.environment.f_ops = 0.50;

config.environment.tau_direct_ns = 0.0;
config.environment.tau_pps_ns = 0.125;
config.environment.tau_ops_ns = 3.0;

config.environment.ops_2g_fraction = 0.99;
config.environment.ops_3g_fraction = 0.01;

config.delay_mode =
    PsSourceDelayMode::Exponential;

config.enable_three_gamma = true;

config.three_gamma_model =
    PsSourceThreeGammaModel::Geant4OrePowell;

physics->RegisterPhysics(
    new PsSourceAnnihilationPhysics(config)
);
```

This fixed environment is used whenever `config.environment_provider` is `nullptr`.

The values are application-supplied model parameters. PsSource does not infer these fractions or lifetimes from the Geant4 material.

### `FixedPsSourceEnvironmentProvider`

The same environment can be supplied through the provider interface:

```cpp id="6scux2"
#include "FixedPsSourceEnvironmentProvider.hh"
#include "PsSourceAnnihilationPhysics.hh"

PsEnvironment environment;

environment.medium_id = 1;

environment.f_direct = 0.30;
environment.f_pps = 0.20;
environment.f_ops = 0.50;

environment.tau_direct_ns = 0.0;
environment.tau_pps_ns = 0.125;
environment.tau_ops_ns = 3.0;

environment.ops_2g_fraction = 0.99;
environment.ops_3g_fraction = 0.01;

FixedPsSourceEnvironmentProvider environment_provider(
    environment
);

PsSourceAnnihilationConfig config;

config.environment_provider =
    &environment_provider;

config.delay_mode =
    PsSourceDelayMode::Exponential;

config.enable_three_gamma = true;

config.three_gamma_model =
    PsSourceThreeGammaModel::Geant4OrePowell;

physics->RegisterPhysics(
    new PsSourceAnnihilationPhysics(config)
);
```

`config.environment_provider` is a non-owning pointer. In this example, `environment_provider` must remain alive until the Geant4 run manager no longer uses the registered PsSource physics.

A fixed provider and the equivalent fixed fallback configuration produce the same annihilation behavior.

### Host-defined local environment provider

A host application can resolve the environment from the transported terminal state:

```cpp id="afl9xx"
#include "PsSourceAnnihilationPhysics.hh"
#include "PsSourceEnvironmentProvider.hh"

class CopyNumberEnvironmentProvider
    : public IPsSourceEnvironmentProvider {
public:
    CopyNumberEnvironmentProvider(
        int selected_copy_number,
        const PsEnvironment& selected_environment,
        const PsEnvironment& default_environment
    )
        : m_selected_copy_number(selected_copy_number),
          m_selected_environment(selected_environment),
          m_default_environment(default_environment)
    {
    }

    PsEnvironment ResolveEnvironment(
        const PsTerminalState& terminal_state
    ) const override
    {
        if (
            terminal_state.copy_number ==
            m_selected_copy_number
        ) {
            return m_selected_environment;
        }

        return m_default_environment;
    }

private:
    int m_selected_copy_number;
    PsEnvironment m_selected_environment;
    PsEnvironment m_default_environment;
};
```

Register the provider with the physics configuration:

```cpp id="i94653"
PsEnvironment default_environment;

default_environment.medium_id = 100;
default_environment.f_direct = 1.0;
default_environment.f_pps = 0.0;
default_environment.f_ops = 0.0;
default_environment.ops_2g_fraction = 1.0;
default_environment.ops_3g_fraction = 0.0;

PsEnvironment selected_environment;

selected_environment.medium_id = 200;
selected_environment.f_direct = 0.0;
selected_environment.f_pps = 0.0;
selected_environment.f_ops = 1.0;
selected_environment.tau_ops_ns = 3.0;
selected_environment.ops_2g_fraction = 0.0;
selected_environment.ops_3g_fraction = 1.0;

CopyNumberEnvironmentProvider environment_provider(
    1,
    selected_environment,
    default_environment
);

PsSourceAnnihilationConfig config;

config.environment_provider =
    &environment_provider;

config.delay_mode =
    PsSourceDelayMode::Exponential;

config.enable_three_gamma = true;

config.three_gamma_model =
    PsSourceThreeGammaModel::Geant4OrePowell;

physics->RegisterPhysics(
    new PsSourceAnnihilationPhysics(config)
);
```

A provider may use any relevant `PsTerminalState` information, including:

* `material`;
* `region`;
* `logical_volume`;
* `physical_volume`;
* `copy_number`;
* `touchable_copy_numbers`;
* terminal position;
* terminal time;
* positron track information.

The mapping from Geant4 state to physical positronium parameters belongs to the host application. PsSource supplies the terminal-state interface but does not impose a material database or copy-number convention.

---

## Optional annihilation observer

PsSource does not require host-specific truth infrastructure. Applications that need annihilation metadata may provide an implementation of `IPsSourceAnnihilationObserver`.

```cpp
#include "PsSourceAnnihilationObserver.hh"
#include "PsSourceAnnihilationPhysics.hh"

#include <iostream>

class MyAnnihilationObserver
    : public IPsSourceAnnihilationObserver {
public:
    void OnPsSourceAnnihilation(
        const PsSourceAnnihilationRecord& record
    ) override
    {
        std::cout
            << "Ps class: "
            << static_cast<int>(record.ps_class)
            << ", photons: "
            << record.annihilation_mode
            << ", terminal time: "
            << record.positron_terminal_time_ns
            << " ns, Ps delay: "
            << record.sampled_ps_delay_ns
            << " ns, annihilation time: "
            << record.annihilation_time_ns
            << " ns\n";
    }
};
```

Attach the observer through the physics configuration:

```cpp
MyAnnihilationObserver observer;

PsSourceAnnihilationConfig config;

config.observer = &observer;

physics->RegisterPhysics(
    new PsSourceAnnihilationPhysics(config)
);
```

`config.observer` is a non-owning pointer. The observer must remain alive for as long as the Geant4 run manager may invoke the registered PsSource physics.

The observer receives realized annihilation metadata, including:

* positronium class;
* two- or three-photon multiplicity;
* transported terminal time;
* sampled positronium delay;
* final annihilation time and position;
* local medium identifier and lifetime;
* model name, version, and validation status.

The observer is informational only. It does not create, modify, own, or transport the generated photons.

## Geant4 secondary-photon behavior

PsSource creates annihilation photons as ordinary Geant4 secondary tracks produced by the PsSource annihilation process.

Consequently:

* their parent is the transported positron track;
* their creator process is the registered PsSource annihilation process;
* their vertex position is the transported terminal positron position;
* their vertex time is the terminal positron global time plus the sampled Ps delay;
* their energy, direction, and optional polarization come from the selected annihilation model;
* Geant4 transports them normally after creation.

The host application may process these photons using its existing sensitive detectors, scorers, stepping actions, tracking actions, digitizers, and output systems. No PsSource-specific downstream interface is required.

---

## Build and install

PsSource requires:

* CMake 3.20 or later;
* a C++17 compiler;
* a Geant4 installation discoverable by CMake.

Configure and build PsSource:

```bash
git clone https://github.com/parkerjg/PsSource.git
cd PsSource

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGeant4_DIR=/path/to/Geant4/lib/cmake/Geant4

cmake --build build -j
```

Run the integration tests:

```bash
ctest \
  --test-dir build \
  --output-on-failure
```

Install the package:

```bash
cmake --install build \
  --prefix /path/to/pssource-install
```

The installation provides:

* the `PsSource` library;
* the seven public headers under `include/PsSource`;
* the exported CMake target `PsSource::PsSource`;
* `PsSourceConfig.cmake`;
* `PsSourceConfigVersion.cmake`.

## Integrating PsSource into another CMake project

A downstream project can locate the installed package with:

```cmake
cmake_minimum_required(VERSION 3.20)

project(
    MyGeant4Application
    LANGUAGES CXX
)

find_package(Geant4 REQUIRED)
find_package(PsSource REQUIRED)

add_executable(
    my_application
    main.cc
)

target_compile_features(
    my_application
    PRIVATE
        cxx_std_17
)

target_link_libraries(
    my_application
    PRIVATE
        PsSource::PsSource
)
```

Configure the downstream project with the PsSource installation prefix:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/pssource-install \
  -DGeant4_DIR=/path/to/Geant4/lib/cmake/Geant4

cmake --build build -j
```

`find_package(PsSource REQUIRED)` also resolves the package’s Geant4 dependency. The explicit `find_package(Geant4 REQUIRED)` call in the host project remains useful when the application directly uses Geant4 targets and configuration variables.

---

## Transport-coupled integration examples

The following examples assume that the host application has already created a `G4VModularPhysicsList* physics` and will register `PsSourceAnnihilationPhysics` before passing the physics list to the Geant4 run manager.

### Direct 2γ annihilation with no added delay

```cpp
PsSourceAnnihilationConfig config;

config.environment.f_direct = 1.0;
config.environment.f_pps = 0.0;
config.environment.f_ops = 0.0;

config.environment.tau_direct_ns = 0.0;

config.environment.ops_2g_fraction = 1.0;
config.environment.ops_3g_fraction = 0.0;

config.delay_mode =
    PsSourceDelayMode::Fixed;

config.fixed_delay_ns = 0.0;

config.enable_three_gamma = false;

physics->RegisterPhysics(
    new PsSourceAnnihilationPhysics(config)
);
```

Geant4 transports the positron to its terminal state. PsSource then creates two back-to-back annihilation photons at the transported terminal position and terminal global time.

### Para-positronium 2γ with exponential delay

```cpp
PsSourceAnnihilationConfig config;

config.environment.f_direct = 0.0;
config.environment.f_pps = 1.0;
config.environment.f_ops = 0.0;

config.environment.tau_pps_ns = 0.125;

config.environment.ops_2g_fraction = 1.0;
config.environment.ops_3g_fraction = 0.0;

config.delay_mode =
    PsSourceDelayMode::Exponential;

config.enable_three_gamma = false;

physics->RegisterPhysics(
    new PsSourceAnnihilationPhysics(config)
);
```

The annihilation time is the transported terminal positron time plus an exponentially sampled p-Ps delay.

### Ortho-positronium 2γ

```cpp
PsSourceAnnihilationConfig config;

config.environment.f_direct = 0.0;
config.environment.f_pps = 0.0;
config.environment.f_ops = 1.0;

config.environment.tau_ops_ns = 3.0;

config.environment.ops_2g_fraction = 1.0;
config.environment.ops_3g_fraction = 0.0;

config.delay_mode =
    PsSourceDelayMode::Exponential;

config.enable_three_gamma = true;

physics->RegisterPhysics(
    new PsSourceAnnihilationPhysics(config)
);
```

This configuration represents delayed o-Ps 2γ annihilation, including application-supplied parameterizations intended to represent pickoff-dominated conditions.

PsSource does not calculate the microscopic pickoff rate from the Geant4 material.

### Ortho-positronium 3γ with Ore–Powell kinematics

```cpp
PsSourceAnnihilationConfig config;

config.environment.f_direct = 0.0;
config.environment.f_pps = 0.0;
config.environment.f_ops = 1.0;

config.environment.tau_ops_ns = 3.0;

config.environment.ops_2g_fraction = 0.0;
config.environment.ops_3g_fraction = 1.0;

config.delay_mode =
    PsSourceDelayMode::Exponential;

config.enable_three_gamma = true;

config.three_gamma_model =
    PsSourceThreeGammaModel::Geant4OrePowell;

physics->RegisterPhysics(
    new PsSourceAnnihilationPhysics(config)
);
```

This selects the native Geant4 Ore–Powell model for the three-photon final-state kinematics.

### Polarized Ore–Powell 3γ

```cpp
config.three_gamma_model =
    PsSourceThreeGammaModel::
        Geant4PolarizedOrePowell;
```

The polarized backend assigns Geant4 photon polarization vectors in addition to the Ore–Powell photon energies and directions.

### Mixed environment

```cpp
PsSourceAnnihilationConfig config;

config.environment.medium_id = 10;

config.environment.f_direct = 0.30;
config.environment.f_pps = 0.20;
config.environment.f_ops = 0.50;

config.environment.tau_direct_ns = 0.0;
config.environment.tau_pps_ns = 0.125;
config.environment.tau_ops_ns = 3.0;

config.environment.ops_2g_fraction = 0.99;
config.environment.ops_3g_fraction = 0.01;

config.delay_mode =
    PsSourceDelayMode::Exponential;

config.enable_three_gamma = true;

config.three_gamma_model =
    PsSourceThreeGammaModel::Geant4OrePowell;

physics->RegisterPhysics(
    new PsSourceAnnihilationPhysics(config)
);
```

The configured fractions describe the application-selected environment model. They are not universal material constants and are not automatically inferred by PsSource.

---

## Validation

The reusable transport-coupled integration path is validated independently of the legacy `PositroniumTruthInfo` infrastructure.

From the repository root:

```bash
cmake --build build-cmake -j

ctest \
  --test-dir build-cmake \
  --output-on-failure
```

The CTest suite covers:

* standalone transport-coupled integration;
* fixed environment-provider integration;
* local environment resolution from `PsTerminalState`.

Run the complete smoke and installed-package tests with:

```bash
./build_smoke.sh
```

This validates:

* the reusable `PsSource` library target;
* installation of the seven public headers;
* export of `PsSource::PsSource`;
* downstream use through `find_package(PsSource REQUIRED)`;
* transport-coupled execution without a PsSource generator, event action, tracking action, or truth object.

Run the transport-coupled regression suite with:

```bash
./run_transport_coupled_regression.sh 100 10000
```

The first argument is the number of events used for each deterministic case. The optional second argument enables the exponential-delay statistical cases.

The suite currently checks:

* direct 2γ with zero added delay;
* fixed configuration and fixed-provider equivalence;
* operation without `PositroniumTruthInfo`;
* truth-enabled and truth-disabled photon equivalence;
* p-Ps 2γ with fixed delay;
* o-Ps 2γ with fixed delay;
* o-Ps 3γ with fixed delay;
* p-Ps exponential timing;
* o-Ps exponential timing;
* transported terminal position and time;
* parentage and creator-process provenance;
* photon multiplicity;
* two-photon and three-photon kinematics;
* model identity and validation metadata.

The regression applications use CSV output to inspect the realized physics, but CSV output is not part of the public PsSource integration contract. A host application may use its own scoring, truth, and output infrastructure.

## Physics-model validation

Additional repository workflows validate the available three-photon models:

```bash
./run_three_gamma_regression.sh 5000
```

This exercises:

* approximate controlled phase-space generation;
* native Geant4 Ore–Powell generation;
* native Geant4 polarized Ore–Powell generation;
* photon multiplicity;
* energy and momentum closure;
* angular isotropy;
* polarization behavior;
* model provenance.

The Ore–Powell implementation also has independent analytic and cross-framework validation workflows comparing PsSource with native Geant4 and GATE.

The approximate phase-space model is retained for controlled software and regression studies. It is not presented as a QED-accurate replacement for the Ore–Powell model.

---

## Repository applications and validation tools

The repository contains applications, scripts, and analysis tools used to develop and validate PsSource.

These include:

* transport-coupled standalone examples;
* explicit source-generation applications;
* timing and truth-output applications;
* deterministic and statistical regression suites;
* Ore–Powell analytic validation;
* native Geant4 and GATE comparison workflows;
* isotropy and polarization validation;
* manuscript figure and table export utilities.

These tools are not required by a downstream application using the installed `PsSource::PsSource` target.

The principal public examples are:

* `standalone_transport_example.cc` — minimal transport-coupled integration using fixed configuration;
* `standalone_environment_provider_example.cc` — integration using `FixedPsSourceEnvironmentProvider`;
* `standalone_local_environment_example.cc` — host-defined local environment resolution from `PsTerminalState`.

The legacy applications based on `PositroniumGenerator` and `PositroniumProvider` remain available for controlled source studies and validation, but they should not be copied as the default integration architecture for an existing Geant4 application.

## Development validation commands

From a configured repository build:

```bash
cmake --build build-cmake -j

ctest \
  --test-dir build-cmake \
  --output-on-failure

./build_smoke.sh

./run_transport_coupled_regression.sh 100 10000
```

Additional three-photon validation workflows include:

```bash
./run_three_gamma_regression.sh 5000

./run_ore_powell_benchmark.sh

./run_polarized_ore_powell_benchmark.sh

./run_cross_framework_ore_powell_comparison.sh
```

Some benchmark scripts require previously prepared reference environments, containers, or datasets described in their corresponding repository documentation.

---

## Scope and current limitations

PsSource is a configurable terminal-annihilation physics extension. It is not a complete microscopic model of positronium formation in matter.

Current limitations are:

* PsSource takes over at the terminal Geant4 at-rest annihilation stage. It does not model microscopic positronium formation during in-flight positron transport.
* Branch fractions and lifetime parameters are supplied by the host application or environment provider.
* PsSource does not infer positronium behavior from elemental composition, Geant4 material name, density, or tissue type.
* Correlations among direct, p-Ps, o-Ps 2γ, and o-Ps 3γ components must be enforced by the application’s environment model.
* The fixed parameterization does not model collision-by-collision pickoff, spin conversion, chemical quenching, or diffusion.
* The same configured o-Ps lifetime is currently used for both the o-Ps 2γ and o-Ps 3γ branches.
* Magnetic-substate-resolved positronium polarization is not currently exposed.
* `PsTerminalState::source_event_id` is not yet a general host-event identifier in the transport-coupled path.
* Provider and observer execution must follow the thread-safety requirements of the host Geant4 application.
* The current package is built as a static library. Shared-library packaging and broader release automation are deferred.
* Application-specific environment databases, material mappings, region mappings, phantom-property maps, and field-dependent models remain host responsibilities.

These limitations do not prevent the validated use of PsSource as a transport-coupled terminal-annihilation extension. They define the boundary between the current software and future environment-specific or microscopic physics development.

## Recommended and legacy use

The recommended use is:

> Register `PsSourceAnnihilationPhysics` with an existing Geant4 modular physics list and allow the host application to retain its normal positron source, transport, detector, scoring, and analysis systems.

The repository also retains a legacy explicit source-generator path. That path is useful for:

* controlled pure-branch datasets;
* analytic and cross-framework physics validation;
* regression testing;
* manuscript reproducibility;
* detector and reconstruction sensitivity studies.

The legacy source-generator classes are repository implementation tools and are not part of the installed seven-header public API.

## Scientific interpretation

PsSource separates two distinct responsibilities:

1. **Annihilation physics**, including two-photon or validated Ore–Powell three-photon final-state generation.
2. **Environmental parameterization**, including branch fractions and lifetime values supplied by the host application.

Validation of the annihilation models does not validate an application’s chosen environmental parameters. Users are responsible for documenting the physical basis, measurements, literature values, or assumptions used to construct each `PsEnvironment`.

---

## Release

The current public release is PsSource 3.0.0.

```text
Tag: pssource-v3.0.0
Commit: 0f965f0
Validated Geant4 baseline: 11.3.2
```

The `3.0.0` version identifies the third-generation transport-coupled PsSource architecture.

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
