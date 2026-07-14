# PsSource Public Integration Contract

## 1. Purpose

PsSource is a modular Geant4 extension for positronium-aware terminal positron annihilation.

Its purpose is to allow an existing Geant4 application to enable or disable configurable positronium modeling without requiring changes to the host application’s:

- primary-particle source;
- geometry;
- material definitions;
- particle transport before annihilation;
- detector construction;
- sensitive detectors;
- scoring;
- digitization;
- event reconstruction;
- analysis pipeline;
- output infrastructure.

In transport-coupled mode, the host application creates and transports the positron normally. PsSource acts only when the transported positron reaches the terminal Geant4 at-rest annihilation stage.

PsSource then:

1. captures the terminal positron state;
2. resolves the applicable positronium environment;
3. selects direct annihilation, para-positronium, or ortho-positronium behavior;
4. samples the applicable positronium delay;
5. selects the two-photon or three-photon final state;
6. creates ordinary Geant4 photon secondaries;
7. optionally records annihilation truth and model provenance.

The generated photons are returned to ordinary Geant4 tracking and interact with the host geometry and detector systems without requiring PsSource-specific downstream handling.

This document defines the intended public integration behavior of PsSource. It is the design contract against which implementation, refactoring, testing, documentation, release decisions, and scientific claims should be evaluated.

---

## 2. Scope

This contract applies primarily to the reusable transport-coupled integration path.

PsSource may also continue to provide explicit event-generation applications and validation tools. Those applications are useful for:

- controlled source studies;
- analytic validation;
- cross-framework comparisons;
- model-development work;
- regression testing;
- reproducibility studies;
- detector and reconstruction sensitivity studies.

However, explicit event generation is not the primary external integration interface defined by this contract.

The primary external-use scenario is:

> A researcher already has a functioning Geant4 application and wants to enable positronium-aware annihilation physics without redesigning the application’s source, geometry, transport, scoring, detector response, or output pipeline.

PsSource is not restricted to PET or imaging applications.

Potential host applications include:

- PET and positronium-sensitive imaging simulations;
- positron-beam experiments;
- accelerator-target studies;
- antimatter experiments;
- positronium lifetime spectroscopy;
- material-characterization studies;
- condensed-matter simulations;
- detector-development experiments;
- polarization and symmetry studies;
- fundamental two-photon and three-photon annihilation studies;
- any Geant4 application that produces transported positrons.

PET is the principal initial application domain and validation context, but it is not a requirement of the PsSource software architecture.

---

## 3. Intended Host Application

The intended host application may contain any combination of:

- a custom `G4VUserPrimaryGeneratorAction`;
- `G4GeneralParticleSource`;
- Geant4 radioactive decay;
- accelerator-generated positrons;
- positron beams;
- radionuclide sources;
- multiple primary particles;
- nonzero primary times;
- arbitrary source locations;
- arbitrary positron kinetic-energy distributions;
- custom or reference Geant4 physics lists;
- homogeneous or heterogeneous materials;
- voxelized geometries;
- magnetic or electric fields;
- imaging systems;
- spectroscopy systems;
- beamline instrumentation;
- particle detectors;
- scintillation detectors;
- semiconductor detectors;
- calorimeters;
- sensitive detectors;
- primitive scorers;
- optical-photon transport;
- parameterized detector response;
- digitization;
- reconstruction;
- application-specific truth and analysis output.

PsSource must not require the host application to use:

- `PositroniumGenerator`;
- `TimedEventSpec`;
- `main_timing.cc`;
- the current PsSource command-line parser;
- a particular detector class;
- a particular sensitive-detector name;
- a particular output format;
- a particular source implementation;
- a particular geometry;
- a particular event-action implementation;
- PET-specific scoring or reconstruction code.

---

## 4. External User Experience

The intended external integration should be conceptually similar to:

```cpp
#include "PsSourceAnnihilationPhysics.hh"

#include "G4PhysListFactory.hh"
#include "G4VModularPhysicsList.hh"

G4PhysListFactory factory;

G4VModularPhysicsList* physics_list =
    factory.GetReferencePhysList("FTFP_BERT");

if (enable_pssource) {
    PsSourceAnnihilationConfig config;

    config.environment.medium_id = 1;

    config.environment.f_direct = 0.60;
    config.environment.f_pps = 0.10;
    config.environment.f_ops = 0.30;

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

    physics_list->RegisterPhysics(
        new PsSourceAnnihilationPhysics(config)
    );
}

run_manager->SetUserInitialization(
    physics_list
);
```

These class names and enumerations define the current installed public API.

When PsSource is not registered:

* native Geant4 positron annihilation remains active;
* no PsSource configuration is required;
* no PsSource truth is produced;
* the host application behaves as it did before PsSource was added.

When PsSource is registered:

* Geant4 still performs positron transport;
* PsSource replaces terminal at-rest positron annihilation;
* direct, para-positronium, and ortho-positronium behavior become configurable;
* positronium delay is applied after positron transport;
* generated photons continue through ordinary Geant4 transport;
* the host geometry, detector, scoring, and analysis pipeline remain unchanged.

---

## 5. Activation Model

PsSource activation should occur through Geant4 physics registration.

Recommended pattern:

```cpp
if (enable_pssource) {
    physics_list->RegisterPhysics(
        new PsSourceAnnihilationPhysics(config)
    );
}
```

PsSource should not rely on a hidden event-by-event Boolean that leaves native and PsSource terminal annihilation processes simultaneously active.

Registration-level activation is preferred because it provides a clear and auditable distinction between:

* native Geant4 annihilation;
* PsSource terminal annihilation.

The host application should be able to compile with PsSource support while choosing at runtime whether to register it.

---

## 6. Responsibility Boundary

### 6.1 PsSource Responsibilities

PsSource is responsible for the positronium-aware terminal-annihilation layer.

PsSource owns:

* replacement of terminal at-rest positron annihilation;
* capture of the transported positron terminal state;
* environment resolution at the terminal state;
* direct / para-positronium / ortho-positronium selection;
* fixed or exponential delay sampling;
* ortho-positronium two-photon versus three-photon selection;
* two-photon final-state generation;
* three-photon final-state generation;
* optional photon-polarization assignment;
* model identity and provenance;
* optional annihilation truth;
* creation of valid Geant4 photon secondaries;
* return of those secondaries to ordinary Geant4 tracking.

### 6.2 Host Application Responsibilities

The host application remains responsible for:

* creation of the primary positron, radionuclide, or parent particle;
* radionuclide decay;
* positron initial energy;
* positron initial position;
* positron initial time;
* positron transport before terminal annihilation;
* geometry;
* material definitions;
* electromagnetic transport outside terminal annihilation;
* external fields;
* detector construction;
* sensitive detectors;
* scoring;
* optical-photon production and transport;
* photodetector response;
* digitization;
* electronics modeling;
* trigger logic;
* coincidence processing;
* event reconstruction;
* image reconstruction;
* spectroscopy analysis;
* application-level output;
* user-specific scientific interpretation.

### 6.3 Boundary Rule

PsSource must not take ownership of host systems outside terminal positron annihilation.

The host application must not be required to understand the internal PsSource final-state generator in order to transport, score, detect, or analyze the resulting photons.

---

## 7. Transport-Coupled Handoff

The public transport-coupled workflow is:

```text
Host creates positron
        ↓
Geant4 transports positron
        ↓
Positron reaches terminal at-rest state
        ↓
PsSource captures terminal state
        ↓
PsSource resolves local environment
        ↓
PsSource samples realized class
        ↓
PsSource samples delay
        ↓
PsSource generates 2γ or 3γ secondaries
        ↓
Geant4 transports photons normally
```

PsSource acts at the Geant4 at-rest annihilation stage.

The current modeled annihilation position is:

```text
transported positron terminal position
```

The current modeled annihilation time is:

```text
transported positron terminal global time
+
sampled positronium delay
```

No positronium atom spatial transport is included in the current contract.

---

## 8. Terminal-State Requirements

The Geant4 integration layer should construct a terminal-state object from the transported positron.

A conceptual public structure is:

```cpp
struct PsTerminalState {
    G4ThreeVector position;
    G4ThreeVector momentum_direction;

    double global_time_ns = 0.0;
    double kinetic_energy_mev = 0.0;

    int track_id = -1;
    int parent_id = -1;

    const G4Material* material = nullptr;
    const G4Region* region = nullptr;
    const G4LogicalVolume* logical_volume = nullptr;
    const G4VPhysicalVolume* physical_volume = nullptr;

    int copy_number = -1;
};
```

The exact representation may evolve, but the available information should include, when available:

* terminal position;
* terminal global time;
* terminal kinetic energy;
* terminal momentum direction;
* positron track ID;
* positron parent ID;
* material;
* region;
* logical volume;
* physical volume;
* copy number;
* touchable hierarchy or equivalent geometry context.

The terminal-state object should be independent of application-specific truth and output code.

---

## 9. Public Configuration

The installed public configuration types are defined in `PsSourceTypes.hh` and `PsSourceAnnihilationConfig.hh`.

### 9.1 Environment configuration

```cpp
struct PsEnvironment {
    int medium_id = 0;

    double f_direct = 0.3;
    double f_pps = 0.2;
    double f_ops = 0.5;

    double tau_direct_ns = 0.0;
    double tau_pps_ns = 0.125;
    double tau_ops_ns = 3.0;

    double ops_2g_fraction = 0.0;
    double ops_3g_fraction = 1.0;
};
```

`PsEnvironment` defines the application-supplied local positronium parameterization used for one terminal annihilation.

It contains:

* the direct, p-Ps, and o-Ps branch fractions;
* the lifetime associated with each Ps class;
* the o-Ps 2γ and 3γ branch fractions;
* an application-defined medium identifier.

PsSource does not infer these values from Geant4 material composition.

### 9.2 Delay modes

```cpp
enum class PsSourceDelayMode {
    Fixed,
    Exponential
};
```

Semantics:

* `Fixed` applies `fixed_delay_ns` to the realized branch.
* `Exponential` samples the delay using the lifetime associated with the realized `PsClass`.

For direct annihilation, exponential mode uses `tau_direct_ns`. A value of zero produces no additional PsSource delay.

### 9.3 Three-photon models

```cpp
enum class PsSourceThreeGammaModel {
    ApproximatePhaseSpace,
    Geant4OrePowell,
    Geant4PolarizedOrePowell
};
```

Meanings:

* `ApproximatePhaseSpace` provides controlled approximate three-photon generation for software development and regression studies.
* `Geant4OrePowell` uses the native Geant4 Ore–Powell model.
* `Geant4PolarizedOrePowell` uses the native polarized Geant4 Ore–Powell model.

The approximate model must not be represented as equivalent to the validated Ore–Powell models.

### 9.4 Top-level configuration

```cpp
struct PsSourceAnnihilationConfig {
    PsEnvironment environment;

    const IPsSourceEnvironmentProvider*
        environment_provider = nullptr;

    PsSourceDelayMode delay_mode =
        PsSourceDelayMode::Exponential;

    double fixed_delay_ns = 3.0;

    bool enable_three_gamma = true;

    PsSourceThreeGammaModel three_gamma_model =
        PsSourceThreeGammaModel::
            ApproximatePhaseSpace;

    IPsSourceAnnihilationObserver*
        observer = nullptr;
};
```

When `environment_provider` is null, PsSource uses the fixed `environment` value.

When `environment_provider` is non-null, PsSource calls it with the transported `PsTerminalState` to resolve the environment for that annihilation.

The `environment_provider` and `observer` pointers are non-owning. The host application must keep the referenced objects alive for as long as the Geant4 run manager may use the registered PsSource physics.

The observer is optional and does not affect the generated annihilation physics.

---

## 10. Configuration Validation

Configuration must be validated during initialization.

Invalid configurations should not survive until event processing.

### 10.1 Fraction Rules

Required:

```text
f_direct >= 0
f_pps >= 0
f_ops >= 0
f_direct + f_pps + f_ops > 0
```

The implementation must adopt and document one of the following policies:

1. require the fractions to sum to one within tolerance; or
2. normalize positive fractions internally.

The preferred initial policy is internal normalization.

When normalization occurs, initialization diagnostics should report both:

* user-supplied fractions;
* normalized fractions.

### 10.2 Lifetime Rules

Required:

```text
tau_pps_ns > 0
tau_ops_ns > 0
```

### 10.3 Fixed Delay Rule

Required:

```text
fixed_delay_ns >= 0
```

### 10.4 Ortho-Positronium Branch Rule

Required:

```text
0 <= ortho_3g_fraction <= 1
```

### 10.5 Model Availability

If the selected backend is unavailable, unsupported, or improperly configured, initialization must fail with a clear message.

### 10.6 Numeric Validity

Configuration values must be finite.

NaN and infinite values must be rejected.

---

## 11. Realized Positronium Class Semantics

The numeric meanings of realized positronium classes are part of the public truth contract.

```cpp
enum class PsClass : int {
    Direct2g = 0,
    ParaPs2g = 1,
    OrthoPs2g = 2,
    OrthoPs3g = 3
};
```

These values must remain stable.

| Numeric ID | Enum        | Meaning                                  |
| ---------: | ----------- | ---------------------------------------- |
|          0 | `Direct2g`  | direct two-photon annihilation           |
|          1 | `ParaPs2g`  | para-positronium two-photon annihilation |
|          2 | `OrthoPs2g` | ortho-positronium two-photon branch      |
|          3 | `OrthoPs3g` | ortho-positronium three-photon branch    |

New class meanings must not reuse existing values.

Breaking changes to these numeric semantics require a major-version change after the stable public release.

---

## 12. Source Independence

In transport-coupled mode, PsSource must not generate or require the primary positron.

PsSource must support positrons originating from:

* custom primary generators;
* `G4GeneralParticleSource`;
* Geant4 radioactive decay;
* accelerator beamlines;
* secondary-particle production;
* multiple source locations;
* multiple source times;
* realistic positron energy distributions;
* arbitrary host geometries.

PsSource must not depend on:

* `PositroniumGenerator`;
* `TimedEventSpec`;
* current CLI parsing;
* `main_timing.cc`;
* current timing-test geometry;
* PET-specific source conventions;
* current detector names.

Explicit event-generation applications may continue to use specialized source components. Those components must not be required by the reusable transport-coupled physics extension.

---

## 13. Geometry and Detector Independence

PsSource-generated photons must be ordinary Geant4 photon secondaries.

Each generated photon must have:

* particle type `G4Gamma`;
* valid positive kinetic energy;
* normalized direction;
* valid global time;
* valid creation position;
* correct positron parent linkage where supported;
* normal Geant4 tracking and interaction behavior.

PsSource must not require:

* a specific geometry;
* a specific detector technology;
* a specific sensitive detector;
* a specific detector name;
* a specific hit collection;
* a PET-specific detector class;
* an application-specific detector or reconstruction class;
* a particular scoring action;
* a particular output schema.

The same PsSource integration should be usable with:

* PET scanners;
* total-body imaging systems;
* small-animal imaging systems;
* positron-beam experiments;
* accelerator-target experiments;
* calorimeters;
* scintillation detectors;
* semiconductor detectors;
* spectroscopy setups;
* generic Geant4 scoring geometries.

---

## 14. Environment Provider Interface

PsSource supports an optional public environment-provider interface for resolving local positronium parameters from the transported terminal positron state.

The installed interface is:

```cpp
class IPsSourceEnvironmentProvider {
public:
    virtual ~IPsSourceEnvironmentProvider() = default;

    virtual PsEnvironment ResolveEnvironment(
        const PsTerminalState& terminal_state
    ) const = 0;
};
```

The provider is assigned through:

```cpp
PsSourceAnnihilationConfig::environment_provider
```

This pointer is non-owning. The host application must keep the provider alive for as long as the Geant4 run manager may use the registered PsSource physics.

### 14.1 Fixed fallback environment

When `environment_provider` is null, PsSource uses:

```cpp
PsSourceAnnihilationConfig::environment
```

This preserves the simplest fixed-configuration integration path and requires no provider object.

### 14.2 Fixed provider

PsSource installs the convenience provider:

```cpp
FixedPsSourceEnvironmentProvider
```

It stores one `PsEnvironment` value and returns it for every terminal state.

The fixed-provider path and the equivalent direct fixed-configuration path are required to produce identical physics under identical random-engine conditions.

### 14.3 Host-defined providers

A host application may derive from `IPsSourceEnvironmentProvider` and resolve the environment using any relevant part of `PsTerminalState`, including:

* terminal position;
* terminal global time;
* initial or terminal positron kinetic energy;
* track length;
* source-to-terminal displacement;
* material;
* region;
* logical volume;
* physical volume;
* copy number;
* touchable copy-number hierarchy;
* positron track and parent identifiers.

Possible application-defined policies include:

* material-based lookup;
* region-based lookup;
* physical- or logical-volume mapping;
* copy-number mapping;
* voxel or image-property lookup;
* spatial-coordinate lookup;
* target-layer mapping;
* external database lookup;
* user-defined callback logic.

PsSource does not prescribe these mappings as part of its public API.

### 14.4 Resolution point

Environment resolution occurs at the transported terminal positron state immediately before PsSource samples the realized branch, delay, and annihilation final state.

The environment must not be resolved solely from the original positron source location when the host application requires local terminal-state behavior.

### 14.5 No automatic physical inference

PsSource does not automatically infer material-, chemical-, condensed-matter-, biological-, or tissue-specific positronium parameters from generic Geant4 material definitions.

An environment provider supplies an application-defined parameterization. The physical validity of that mapping remains the responsibility of the host application unless a separately validated environment model is explicitly implemented and documented.

---

## 15. Annihilation Observation Is Optional

PsSource physics execution must not depend on application-specific truth or output infrastructure.

PsSource must run successfully when the host application has:

* no custom `G4VUserEventInformation`;
* no PsSource event action;
* no PsSource tracking action;
* no CSV or ROOT writer;
* no annihilation observer;
* no PsSource-specific output.

Optional annihilation observation is enabled through:

```cpp
PsSourceAnnihilationConfig::observer
```

When this pointer is null, PsSource performs the annihilation without issuing an observer callback.

The absence of an observer must not alter:

* branch selection;
* sampled delay;
* photon multiplicity;
* photon energies;
* photon directions;
* photon polarization;
* secondary-track creation.

The observer pointer is non-owning. The host application must keep the observer alive for as long as the Geant4 run manager may invoke the registered PsSource physics.

## 16. Public Annihilation Observer and Record

The installed observer interface is:

```cpp
class IPsSourceAnnihilationObserver {
public:
    virtual ~IPsSourceAnnihilationObserver() = default;

    virtual void OnPsSourceAnnihilation(
        const PsSourceAnnihilationRecord& record
    ) = 0;
};
```

The installed record is:

```cpp
struct PsSourceAnnihilationRecord {
    PsClass ps_class = PsClass::Direct2g;
    int annihilation_mode = 2;

    double positron_terminal_time_ns = 0.0;
    double sampled_ps_delay_ns = 0.0;
    double annihilation_time_ns = 0.0;

    std::array<double, 3> annihilation_position_mm = {
        0.0, 0.0, 0.0
    };

    int medium_id = 0;
    double local_tau_ns = 0.0;

    std::string model_name;
    std::string model_version;
    std::string validation_status;
};
```

The record describes one realized PsSource annihilation and includes:

* the realized positronium class;
* two- or three-photon multiplicity;
* transported terminal positron time;
* sampled Ps delay;
* final annihilation time and position;
* application-defined medium identifier;
* lifetime used for the realized class;
* physics-model identity and validation provenance.

The observer callback does not own or control the generated photons. It receives metadata after the annihilation has been realized.

The public observer API does not prescribe a storage format. A host application may translate the record into CSV, ROOT, HDF5, JSON, database records, or its existing event-truth structures.

Photon-level data are not currently included in `PsSourceAnnihilationRecord`. Applications requiring photon-level output may obtain it from ordinary Geant4 tracking or scoring infrastructure.

---

## 17. Multiple Annihilations per Event

The reusable truth design should support multiple positron annihilations in one Geant4 event.

The long-term truth model should therefore allow:

```cpp
std::vector<PsAnnihilationRecord>
```

rather than assuming only one annihilation record per event.

The design must not prevent:

* multiple positron primaries;
* radioactive decay chains;
* secondary positron production;
* pileup studies;
* multi-source events;
* accelerator events with multiple positrons.

Existing single-annihilation convenience fields may be preserved temporarily for compatibility.

---

## 18. Photon-Level Metadata

Optional photon track metadata may be attached through `G4VUserTrackInformation`.

Conceptual metadata may include:

* annihilation index;
* photon role;
* realized Ps class;
* model identifier;
* environment identifier;
* polarization-valid flag;
* truth-record linkage.

Photon-level metadata must be optional.

The generated photons must remain transportable and physically valid when no custom track information is attached.

---

## 19. Process Replacement Requirements

The PsSource physics constructor must safely replace terminal positron annihilation.

It must:

1. locate the existing positron annihilation process;
2. verify that the process configuration is compatible;
3. remove or replace the native terminal at-rest implementation;
4. register the PsSource replacement;
5. preserve required process ordering;
6. avoid duplicate at-rest annihilation;
7. fail clearly if the host physics list is incompatible.

There must be exactly one active terminal positron annihilation path.

Initialization failure is preferred over silent duplicate physics.

---

## 20. Process Naming

Where downstream Geant4 logic expects the annihilation process name `annihil`, the PsSource replacement should preserve compatible creator-process semantics when practical.

The implementation should document:

* process name;
* process subtype;
* at-rest process ordering;
* in-flight behavior.

Changes that would break ordinary creator-process filtering should be avoided unless justified and documented.

---

## 21. In-Flight Annihilation

The public contract distinguishes terminal at-rest positronium modeling from in-flight annihilation.

PsSource is intended to replace terminal at-rest annihilation.

Native Geant4 in-flight annihilation should remain available unless a future model explicitly replaces it.

In-flight events must not incorrectly receive:

* a Ps class;
* a Ps lifetime delay;
* a terminal-state environment assignment;
* a PsSource three-photon final state.

A future optional truth record should be able to distinguish:

```cpp
enum class PsHandoffStatus {
    NotAppliedInFlight,
    AppliedAtRest
};
```

This enum is illustrative and is not yet a frozen public interface.

Dedicated in-flight validation is required before broad production claims are made.

---

## 22. Generated Secondary Requirements

For each PsSource-generated photon:

* kinetic energy must be physically valid;
* direction must be normalized;
* birth time must equal the realized annihilation time;
* birth position must equal the modeled annihilation position;
* parent linkage should identify the transported positron;
* ordinary Geant4 transport must begin immediately after creation.

### 22.1 Two-Photon Final State

The two-photon final state must satisfy:

* exactly two photons;
* energy consistent with the selected model;
* back-to-back directions for annihilation at rest;
* momentum closure;
* common birth time;
* common birth position.

### 22.2 Three-Photon Final State

The three-photon final state must satisfy:

* exactly three photons;
* positive individual photon energies;
* total energy conservation;
* vector momentum closure;
* normalized directions;
* common birth time;
* common birth position;
* model-specific distributional behavior.

Conservation checks alone do not establish physical correctness. Validated backends must also reproduce the intended physical distribution.

---

## 23. Timing Semantics

The public timing definition is:

```text
annihilation_time
=
positron_terminal_time
+
sampled_ps_delay
```

For fixed-delay mode:

```text
sampled_ps_delay = configured fixed delay
```

For exponential-delay mode:

```text
sampled_ps_delay ~ Exponential(mean = configured lifetime)
```

All sampled delays must be nonnegative.

The generated photons must inherit the realized annihilation time as their Geant4 global birth time.

---

## 24. Spatial Semantics

The current modeled annihilation position is the transported positron terminal position.

```text
annihilation_position
=
positron_terminal_position
```

The current release does not model:

* positronium diffusion;
* positronium thermal motion;
* positronium displacement during lifetime;
* formation before terminal positron rest;
* material-dependent positronium transport range.

Any future spatial positronium model must be added explicitly and must not silently alter the current terminal-position semantics.

---

## 25. Polarization Semantics

Polarization support is model-dependent.

When polarization is not generated:

* the photon must remain valid for ordinary Geant4 transport;
* truth must indicate that polarization is not valid or not assigned;
* zero-vector placeholders must not be represented as physical polarization.

When polarization is generated:

* the polarization vector must be normalized as required;
* the polarization vector must be transverse to the photon direction;
* model-specific polarization validation must pass;
* truth must identify polarization as valid.

The initial public contract does not require magnetic-substate-resolved control.

---

## 26. Model Provenance

Each realized PsSource annihilation should expose, through optional truth, model provenance sufficient to identify:

* model name;
* model version;
* validation status;
* selected backend;
* environment provider;
* relevant configuration identifier.

Provenance should distinguish:

* approximate controlled models;
* validated Ore–Powell models;
* polarized models;
* future material-dependent models;
* user-defined backends.

The implementation must not label an approximate model as a validated physical Ore–Powell implementation.

---

## 27. User-Defined Models

The public architecture should allow future user-defined physics models without requiring changes to the Geant4 process layer.

A conceptual interface is:

```cpp
class IPsPhysicsModel {
public:
    virtual ~IPsPhysicsModel() = default;

    virtual PsModelResult Sample(
        const PsEnvironment& environment
    ) const = 0;

    virtual std::string Name() const = 0;
    virtual std::string Version() const = 0;
    virtual std::string ValidationStatus() const = 0;
};
```

User-defined models must return physically and structurally valid results.

The integration layer must remain responsible for validating:

* photon multiplicity;
* energy positivity;
* direction normalization;
* timing validity;
* model provenance.

---

## 28. Error Handling

### 28.1 Initialization Errors

Fatal initialization errors should be used for:

* invalid fractions;
* invalid lifetimes;
* invalid branch fractions;
* unavailable model backend;
* incompatible physics-list configuration;
* duplicate annihilation process;
* missing required annihilation process;
* unsupported process replacement state;
* null required provider;
* invalid ownership configuration.

### 28.2 Event-Time Errors

Event-time fatal errors should be reserved for conditions that indicate corrupted physics state or impossible execution.

The absence of optional truth must not be an error.

### 28.3 Diagnostic Quality

Errors should identify:

* component;
* invalid value;
* expected range or condition;
* likely corrective action.

---

## 29. Initialization Diagnostics

When PsSource is enabled, initialization should emit a concise summary once.

Example:

```text
PsSource physics integration
  terminal annihilation      : PsSource
  in-flight annihilation     : native Geant4
  environment provider       : uniform
  direct fraction            : 0.60
  para-Ps fraction           : 0.10
  ortho-Ps fraction          : 0.30
  para-Ps lifetime           : 0.125 ns
  ortho-Ps lifetime          : 3.0 ns
  ortho-Ps 3gamma fraction   : 0.01
  delay mode                 : exponential
  3gamma backend             : OrePowell
  polarization               : disabled
  truth recorder             : disabled
```

Diagnostics must not be emitted once per event.

Multithreaded initialization should avoid redundant worker output where practical.

---

## 30. Backward Compatibility

The integration refactor must preserve existing validated behavior.

The following should remain supported unless explicitly deprecated:

* explicit event-generation mode;
* transport-coupled mode;
* approximate three-photon backend;
* Ore–Powell backend;
* polarized Ore–Powell backend;
* fixed delay;
* exponential delay;
* current realized `PsClass` numeric values;
* current regression validators;
* current reference applications.

Refactoring must not silently change validated physics distributions.

Any intentional physics change must include:

* documented rationale;
* new validation;
* updated provenance;
* updated model version.

---

## 31. Existing Application Compatibility

The current PsSource reference applications may continue to provide:

* command-line configuration;
* CSV truth output;
* detector-hit output;
* validation hooks;
* regression data;
* PET-oriented examples.

These application-level features are not mandatory dependencies of the reusable library.

The reusable library must not require:

* current CLI parsing;
* current CSV writers;
* current event-action implementation;
* current tracking-action implementation;
* current detector geometry;
* current PET-specific examples.

---

## 32. Build and Packaging Contract

PsSource is built and installed as a reusable CMake library package.

The repository defines:

```cmake
add_library(
    PsSource
    ...
)

add_library(
    PsSource::PsSource
    ALIAS
    PsSource
)
```

An installed downstream application uses:

```cmake
find_package(PsSource REQUIRED)

target_link_libraries(
    my_geant4_application
    PRIVATE
        PsSource::PsSource
)
```

The installed package provides:

* the compiled PsSource library;
* seven public headers;
* the exported target `PsSource::PsSource`;
* `PsSourceConfig.cmake`;
* `PsSourceConfigVersion.cmake`;
* transitive resolution of the Geant4 dependency.

Application executables, manuscript utilities, CSV validators, regression scripts, and legacy source-generator infrastructure are not part of the required runtime package.

The package must remain usable without copying PsSource source files directly into the host application.

## 33. Public Header Boundaries

The installed public API consists of exactly these seven headers:

```text
FixedPsSourceEnvironmentProvider.hh
PsSourceAnnihilationConfig.hh
PsSourceAnnihilationObserver.hh
PsSourceAnnihilationPhysics.hh
PsSourceEnvironmentProvider.hh
PsSourceTypes.hh
PsTerminalState.hh
```

Their public responsibilities are:

* `PsSourceAnnihilationPhysics.hh` — Geant4 physics registration;
* `PsSourceAnnihilationConfig.hh` — top-level configuration;
* `PsSourceAnnihilationObserver.hh` — optional annihilation observation;
* `PsSourceEnvironmentProvider.hh` — host-defined local environment resolution;
* `FixedPsSourceEnvironmentProvider.hh` — fixed provider convenience implementation;
* `PsSourceTypes.hh` — shared public environment and enum types;
* `PsTerminalState.hh` — transported terminal-state description.

The public API must not require or expose:

* `PositroniumGenerator`;
* `PositroniumProvider`;
* `PositroniumTruthInfo`;
* `TimedEventSpec`;
* internal physics-model implementation headers;
* application command-line parsing;
* CSV schemas;
* detector-specific classes;
* PET-specific reconstruction or scoring;
* validation scripts.

Internal process, model, builder, application, and truth-adapter classes may evolve without expanding the installed public API.

## 34. Ownership and Lifetime

`PsSourceAnnihilationConfig` is copied into the registered PsSource process configuration.

The following pointers are non-owning:

```cpp
const IPsSourceEnvironmentProvider*
    environment_provider;

IPsSourceAnnihilationObserver*
    observer;
```

The host application retains ownership of both objects and must keep them alive for as long as the Geant4 run manager may use the registered PsSource physics.

PsSource does not delete either object.

The Geant4 material, region, logical-volume, and physical-volume pointers exposed through `PsTerminalState` are also non-owning. They refer to objects managed by the host Geant4 application.

`FixedPsSourceEnvironmentProvider` stores its `PsEnvironment` by value and does not retain a reference to the constructor argument.

The host application transfers the newly allocated `PsSourceAnnihilationPhysics` object to Geant4 through `RegisterPhysics()` according to the normal `G4VModularPhysicsList` ownership model.

---

## 35. Thread Safety

PsSource does not introduce mutable global physics state or a separate random-number generator. Stochastic sampling uses the Geant4-managed random stream.

The public environment-provider and observer interfaces may be called from Geant4 worker execution contexts. Therefore, thread safety of host-supplied implementations is the host application’s responsibility.

A host-defined `IPsSourceEnvironmentProvider` should:

* avoid unsynchronized mutation of shared state;
* treat `ResolveEnvironment()` as logically const;
* return each `PsEnvironment` by value;
* synchronize access to external databases or shared caches when required.

A host-defined `IPsSourceAnnihilationObserver` should:

* avoid unsynchronized writes to shared output;
* use thread-local storage, worker-specific output, or explicit synchronization;
* not assume callbacks arrive in event-number order;
* not modify PsSource physics configuration during a run.

`FixedPsSourceEnvironmentProvider` is immutable after construction and stores its environment by value.

The current regression milestone establishes sequential correctness and reproducibility under fixed seeds. Full multithreaded validation, cross-thread-count statistical equivalence, and formal guarantees regarding callback ordering remain deferred work.

Bitwise-identical output across different Geant4 thread counts is not claimed.

---

## 36. Randomness and Reproducibility

All PsSource stochastic sampling uses the Geant4-managed random-number stream.

PsSource does not create or seed an independent random engine.

The current implementation uses Geant4-compatible random sampling for:

* direct, p-Ps, and o-Ps branch selection;
* o-Ps 2γ versus 3γ selection;
* fixed or exponential delay behavior;
* two-photon direction generation;
* approximate three-photon generation;
* native Geant4 Ore–Powell generation;
* native polarized Geant4 Ore–Powell generation.

Under identical sequential execution, configuration, Geant4 version, random engine, and seed state, PsSource is expected to reproduce the same stochastic sequence.

A host application that requires reproducible runs should record at least:

* PsSource version or repository commit;
* Geant4 version;
* random-engine type;
* initial seed or saved engine state;
* thread count;
* event count;
* complete `PsSourceAnnihilationConfig`;
* effective environment-provider policy;
* selected three-photon model.

PsSource does not itself require or provide a run-metadata format. The repository applications write configuration and provenance files for validation, but those files are not part of the installed public API.

Reproducibility is not claimed across:

* different Geant4 versions;
* different random engines;
* different compiler or platform implementations where Geant4 behavior changes;
* different thread counts;
* different event scheduling;
* environment providers whose external state changes during a run.

The current validated reproducibility claims are limited to the sequential deterministic, equivalence, and statistical regression workflows included in the repository.

---

## 37. Versioning Policy

PsSource 3.0.0 is the first public release of the transport-coupled architecture.

The release is identified by:

```text
Version: 3.0.0
Tag: pssource-v3.0.0
Commit: 0f965f0
```

The major version reflects the third-generation PsSource architecture rather than two prior formally packaged public releases.

For all releases beginning with 3.0.0:

* public API changes must be documented explicitly;
* changes to the seven installed headers require integration review;
* changes to default physics behavior require regression and scientific review;
* changes to enum values or record semantics must not be made silently;
* repository commits and release tags should be used to identify reproducible software states.

PsSource follows semantic versioning:

```text
MAJOR.MINOR.PATCH
```

### Major version

Increment for:

* breaking public API changes;
* removal or renaming of public interfaces;
* incompatible configuration changes;
* changed enum or record semantics;
* changed default physics with externally visible consequences.

### Minor version

Increment for backward-compatible additions such as:

* new optional physics models;
* new environment-provider helpers;
* new observer fields;
* new public examples;
* new supported Geant4 versions.

### Patch version

Increment for backward-compatible corrections such as:

* bug fixes;
* build and packaging fixes;
* documentation corrections;
* validation improvements that do not alter intended public behavior.

### Stable numeric semantics

The current `PsClass` values are:

```cpp
Direct2g = 0
ParaPs2g = 1
OrthoPs2g = 2
OrthoPs3g = 3
```

These numeric values are already used by repository validation and legacy output. They should not be renumbered without an explicitly documented breaking change.

The meanings of existing public enum values, configuration fields, and observer-record fields must not be changed silently.

---

## 38. Supported Geant4 Versions

The currently validated PsSource baseline is:

```text
Geant4 11.3.2
```

The following have been built and validated against that baseline:

* the reusable `PsSource` library;
* the seven-header installed public API;
* CTest integration examples;
* installed-package downstream use through `find_package(PsSource REQUIRED)`;
* deterministic transport-coupled regression cases;
* fixed and exponential delay behavior;
* native Geant4 Ore–Powell generation;
* native polarized Geant4 Ore–Powell generation.

PsSource currently makes no formal compatibility claim for other Geant4 versions.

A different Geant4 version may build and run successfully, but it should be treated as unvalidated until the relevant build, integration, transport, and physics regression suites pass.

Before declaring a stable multi-version release, support should be classified explicitly as:

* **validated** — full build, integration, transport, and applicable physics regressions pass;
* **build-tested** — PsSource and a downstream consumer compile, but full regression validation has not been completed;
* **unsupported** — not tested or known to be incompatible.

Compatibility testing must pay particular attention to changes in:

* positron annihilation process registration;
* at-rest process behavior;
* Geant4 secondary-particle construction;
* Ore–Powell model interfaces;
* polarization handling;
* CMake target and package behavior.

The Geant4 version used for a scientific result should be recorded with the PsSource commit or release version.

---

## 39. Scientific Scope, Non-Goals, and Deferred Work

PsSource is a terminal-annihilation physics extension. It is not a complete microscopic simulation of positronium formation, transport, chemistry, or material response.

### 39.1 Terminal-state approximation

The current transport-coupled implementation allows Geant4 to transport the positron normally and applies PsSource at the terminal at-rest annihilation stage.

This is a computational handoff boundary. It does not imply that physical positronium formation occurs only after the positron has reached rest.

The current implementation does not model:

* positronium formation while the positron is in flight;
* transport of a physical positronium atom;
* a finite positronium spatial trajectory;
* positronium diffusion during the sampled lifetime;
* spatial displacement of the annihilation point after the terminal positron handoff.

The transported positron terminal position is retained as the annihilation position.

### 39.2 Environment and material physics

PsSource accepts application-supplied branch fractions and lifetime parameters through `PsEnvironment`.

It does not automatically model or infer:

* chemistry-driven positronium formation;
* material-dependent collision dynamics;
* pickoff rates from elemental composition;
* spin conversion or quenching rates;
* tissue classification;
* biological parameter estimation;
* patient-specific positronium properties;
* relationships among branch fractions and lifetimes.

A host application or future environment model must provide and justify those relationships.

### 39.3 Polarization and spin

The current three-photon backends may provide photon polarization through native Geant4 Ore–Powell models.

The public API does not currently expose:

* positronium magnetic substate;
* magnetic-substate-resolved generation control;
* magnetic-substate evolution;
* external-field-dependent spin evolution;
* entanglement-specific public truth records.

These remain deferred research and API topics.

### 39.4 Host-application functionality

PsSource does not provide or replace:

* radionuclide decay modeling;
* primary-source management;
* detector construction;
* sensitive detectors;
* optical-photon transport;
* detector electronics;
* digitization;
* trigger logic;
* coincidence sorting;
* reconstruction;
* image analysis;
* spectroscopy analysis;
* experiment-specific event selection.

Generated annihilation photons are ordinary Geant4 secondaries and may be processed by the host application’s existing infrastructure.

### 39.5 Deferred integration and release work

The following are not required for the current validated milestone:

* a native GATE plugin;
* a shared-library build;
* multi-version Geant4 validation;
* formal multithreaded regression validation;
* packaged material or tissue databases;
* standard region-, material-, or voxel-mapping providers;
* broader release automation.

Future work must preserve the distinction between validated annihilation physics and application-supplied environmental parameterization.

---

## 40. Acceptance Tests and Validation Status

Acceptance requirements are divided into:

* tests completed for the current transport-coupled milestone;
* tests required before broader compatibility claims are made.

### 40.1 Completed milestone tests

The current implementation has passed the following acceptance tests.

#### Reusable-library build

Validate:

* successful CMake configuration;
* successful compilation of the `PsSource` library;
* successful compilation of repository applications and examples.

Command:

```bash
cmake --build build-cmake -j
```

#### CTest integration examples

Validate:

* standalone transport-coupled operation;
* fixed environment-provider operation;
* local environment resolution from `PsTerminalState`.

Command:

```bash
ctest \
  --test-dir build-cmake \
  --output-on-failure
```

#### Smoke and installed-consumer tests

Validate:

* installation of the library and seven public headers;
* export of `PsSource::PsSource`;
* downstream configuration with `find_package(PsSource REQUIRED)`;
* downstream compilation without repository-private headers;
* execution without a PsSource-specific generator, event action, tracking action, or truth object.

Command:

```bash
./build_smoke.sh
```

#### Deterministic transport tests

Validated cases:

* direct 2γ with zero added delay;
* p-Ps 2γ with fixed delay;
* o-Ps 2γ with fixed delay;
* o-Ps 3γ with fixed delay.

Validated properties include:

* realized `PsClass`;
* photon multiplicity;
* transported terminal position;
* transported terminal global time;
* sampled Ps delay;
* final annihilation time;
* parent positron track;
* creator-process provenance;
* energy and momentum closure;
* model identity and validation metadata.

#### No-truth and observer-independence tests

Validate:

* operation without `PositroniumTruthInfo`;
* no mandatory PsSource event or tracking action;
* truth-enabled and truth-disabled photon output equivalence;
* optional observation does not alter generated photon physics.

#### Environment-provider tests

Validate:

* fixed fallback environment;
* `FixedPsSourceEnvironmentProvider`;
* byte-for-byte equivalence between matching fixed and fixed-provider configurations under controlled deterministic conditions;
* host-defined local environment selection from terminal volume copy number.

#### Statistical delay tests

Validated cases:

* p-Ps exponential delay;
* o-Ps exponential delay.

Validated properties include:

* non-negative sampled delays;
* event-by-event timing decomposition;
* agreement of sampled distributions with configured lifetimes within predefined statistical tolerances.

The principal transport regression command is:

```bash
./run_transport_coupled_regression.sh 100 10000
```

### 40.2 Physics-model validation

Separate validation workflows cover:

* approximate controlled three-photon generation;
* Geant4 Ore–Powell generation;
* polarized Geant4 Ore–Powell generation;
* photon multiplicity;
* energy and momentum closure;
* rotational isotropy;
* polarization normalization and transversality;
* preservation of the Ore–Powell energy distribution;
* comparison with native Geant4 and GATE reference implementations.

These tests validate annihilation-model implementation. They do not validate application-supplied branch fractions, lifetimes, or material mappings.

### 40.3 Tests not yet claimed as complete

The following remain required before making broader release claims:

* dedicated validation of native in-flight annihilation behavior;
* formal multithreaded regression testing at multiple thread counts;
* validation against additional Geant4 versions;
* integration with multiple independent production host applications;
* broader geometry- and detector-independence demonstrations;
* testing with radioactive decay, accelerator-generated positrons, and other realistic source mechanisms.

The architecture is intended to support these cases, but they must not be described as fully validated until dedicated tests are completed.

### 40.4 Acceptance rule for future changes

A change affecting the public API, process registration, terminal-state handling, branch selection, delay sampling, secondary generation, provider resolution, or observer behavior must pass all applicable completed milestone tests before acceptance.

Changes that introduce new compatibility claims must add dedicated regression coverage for those claims.

---

## 41. Reference Integration Examples

The repository includes three minimal transport-coupled examples built against the reusable PsSource library.

### 41.1 Fixed-configuration example

```text
standalone_transport_example.cc
```

This example demonstrates:

* an independent Geant4 application;
* an ordinary transported positron source;
* a reference modular physics list;
* registration of `PsSourceAnnihilationPhysics`;
* fixed `PsEnvironment` configuration;
* operation without `PositroniumGenerator`;
* operation without `PositroniumTruthInfo`;
* ordinary Geant4 tracking of the generated photon secondaries.

### 41.2 Fixed-provider example

```text
standalone_environment_provider_example.cc
```

This example demonstrates:

* `FixedPsSourceEnvironmentProvider`;
* assignment through `PsSourceAnnihilationConfig::environment_provider`;
* the non-owning provider lifetime contract;
* physics equivalent to the corresponding fixed fallback configuration.

### 41.3 Local-provider example

```text
standalone_local_environment_example.cc
```

This example demonstrates:

* a host-defined `IPsSourceEnvironmentProvider`;
* environment resolution from `PsTerminalState`;
* local selection using transported volume copy number;
* application ownership of the mapping policy.

The examples are compiled and executed through CTest:

```bash
ctest \
  --test-dir build-cmake \
  --output-on-failure
```

They are intended to show the recommended integration architecture rather than provide complete production applications.

None of the examples requires:

* `PositroniumGenerator`;
* `PositroniumProvider`;
* `TimedEventSpec`;
* `PositroniumTruthInfo`;
* a PsSource event action;
* a PsSource tracking action;
* PET-specific geometry;
* a PsSource-specific output format.

Additional application-domain examples may later demonstrate radioactive decay, accelerator-produced positrons, PET systems, spectroscopy systems, or more complex environment-provider policies. Those examples are not required to use a different PsSource public interface.

---

## 42. Application-Domain Compatibility

PsSource is designed as an application-independent Geant4 terminal-annihilation extension.

The same public integration path should support host applications involving:

* PET and positronium-sensitive imaging;
* positron beams;
* accelerator-target experiments;
* positronium lifetime spectroscopy;
* material-characterization studies;
* antimatter experiments;
* polarization and symmetry studies;
* detector-development simulations;
* fundamental two-photon and three-photon annihilation studies.

PsSource does not own application-specific:

* primary-source definitions;
* geometry;
* detector models;
* sensitive detectors;
* scoring;
* digitization;
* triggering;
* reconstruction;
* analysis;
* output formats.

Application-specific examples may demonstrate PsSource integration, but they must not introduce application-specific classes or assumptions into the installed public API.

PET is an important validation and use domain, but it is not an architectural requirement.

---

## 43. Documentation Requirements

The public documentation must present PsSource first as an installable transport-coupled Geant4 physics extension.

The README and integration documentation must include:

* CMake build and installation instructions;
* downstream integration using `find_package(PsSource REQUIRED)`;
* linking through `PsSource::PsSource`;
* registration of `PsSourceAnnihilationPhysics`;
* a precise description of the terminal at-rest handoff;
* the seven installed public headers and their responsibilities;
* fixed `PsEnvironment` configuration;
* `FixedPsSourceEnvironmentProvider`;
* a host-defined provider using `PsTerminalState`;
* the non-owning provider and observer lifetime rules;
* optional `IPsSourceAnnihilationObserver` use;
* the `PsClass` numeric values;
* supported delay and three-photon models;
* current validation commands;
* the validated Geant4 version;
* scientific limitations and deferred work;
* model-provenance semantics.

The documentation must state clearly that:

* Geant4 performs ordinary positron transport before PsSource acts;
* PsSource replaces only terminal at-rest positron annihilation;
* generated annihilation photons are ordinary Geant4 secondaries;
* no PsSource generator, event action, tracking action, truth object, or output format is required;
* environmental branch fractions and lifetimes are application-supplied parameters;
* validation of annihilation physics does not validate a host application’s environmental parameterization.

The recommended transport-coupled integration path must be separated clearly from the legacy explicit source-generator path.

Legacy applications and validation tools may be documented for:

* controlled source studies;
* physics-model validation;
* cross-framework comparison;
* regression testing;
* manuscript reproducibility.

They must not be presented as dependencies of the installed public package.

Application-domain examples must be identified as examples rather than software requirements. The same public interface must remain applicable to imaging, accelerator, spectroscopy, materials, antimatter, and fundamental-physics Geant4 applications.

---

## 44. Contract Review Checklist

A proposed PsSource change should be rejected or revised if reviewers cannot answer all applicable questions below.

### 44.1 Activation and process replacement

* Is PsSource enabled only by registering `PsSourceAnnihilationPhysics`?
* Does omitting that registration preserve native Geant4 annihilation behavior?
* Does PsSource replace only terminal positron at-rest annihilation?
* Are native positron transport and native in-flight processes otherwise preserved?

### 44.2 Responsibility boundary

* Does the host application retain ownership of its primary source, geometry, transport, detectors, scoring, digitization, analysis, and output?
* Does the change avoid introducing a dependency on `PositroniumGenerator`, `PositroniumTruthInfo`, or legacy application infrastructure?
* Are generated photons still returned as ordinary Geant4 secondaries?

### 44.3 Terminal-state handoff

* Is the transported terminal positron position preserved?
* Is the transported terminal global time preserved?
* Is the sampled Ps delay added after the transported terminal time?
* Does the documentation avoid claiming that microscopic positronium formation occurs only at rest?

### 44.4 Environment resolution

* Is fixed `PsEnvironment` configuration still supported?
* If a provider is used, does it receive `PsTerminalState`?
* Does PsSource avoid imposing application-specific material, region, volume, copy-number, or database mappings?
* Are environmental fractions and lifetimes identified as application-supplied parameters?
* Does the provider remain non-owning?

### 44.5 Observer behavior

* Is the observer optional?
* Does enabling or disabling it leave photon physics unchanged?
* Does the observer remain non-owning?
* Does the host application retain control of output format and storage?

### 44.6 Public API and packaging

* Does the change preserve `find_package(PsSource REQUIRED)` and `PsSource::PsSource`?
* Does it preserve the seven-header public boundary unless an API expansion is explicitly justified?
* Are public enum, configuration, and record semantics backward compatible?
* Are new internal implementation details kept out of the installed headers?

### 44.7 Physics behavior

* Are direct 2γ, p-Ps 2γ, o-Ps 2γ, and o-Ps 3γ semantics preserved?
* Are fixed and exponential delay meanings preserved?
* Is the approximate three-photon model still identified as approximate?
* Are Ore–Powell and polarized Ore–Powell provenance reported accurately?
* Does the change avoid conflating validated annihilation physics with unvalidated environmental parameterization?

### 44.8 Validation

* Do all applicable CTest, smoke, installed-package, deterministic, no-truth, equivalence, and statistical regressions pass?
* Has dedicated coverage been added for every new compatibility or physics claim?
* Are untested multithreading, Geant4-version, source-type, or host-application claims described as deferred rather than completed?

### 44.9 Scientific limitations

* Is physical Ps atom transport still identified as unmodeled?
* Is in-flight Ps formation still identified as unmodeled?
* Is automatic inference of material- or tissue-specific parameters still excluded?
* Are magnetic-substate-resolved effects still identified as deferred?

A change is contract-compliant only when its implementation, tests, documentation, and scientific claims remain consistent with these answers.

---

## 45. Current Implementation and Acceptance Status

The transport-coupled public integration milestone is implemented at commit `dadc7b1`.

The following contract requirements have been demonstrated:

1. PsSource builds as a reusable CMake library.
2. PsSource installs as a CMake package exporting `PsSource::PsSource`.
3. An independent downstream project builds and runs using `find_package(PsSource REQUIRED)`.
4. The installed public API is limited to seven headers.
5. An existing Geant4 modular physics list can register `PsSourceAnnihilationPhysics`.
6. The host application creates and transports ordinary positrons.
7. PsSource replaces only terminal at-rest positron annihilation.
8. Transported terminal position and global time are preserved.
9. Generated annihilation photons are ordinary Geant4 secondaries.
10. Direct 2γ, p-Ps 2γ, o-Ps 2γ, and o-Ps 3γ behavior is supported.
11. Fixed and exponential delay modes are supported.
12. Fixed `PsEnvironment` configuration remains the default path.
13. `FixedPsSourceEnvironmentProvider` produces physics equivalent to the corresponding fixed configuration.
14. Host-defined environment providers can resolve local parameters from `PsTerminalState`.
15. The reusable core does not depend on `PositroniumTruthInfo`.
16. A host application may run without a PsSource generator, event action, tracking action, observer, or truth object.
17. Optional observer output does not alter generated photon physics.
18. Existing deterministic and statistical transport regressions pass.

The principal acceptance commands are:

```bash
cmake --build build-cmake -j

ctest \
  --test-dir build-cmake \
  --output-on-failure

./build_smoke.sh

./run_transport_coupled_regression.sh 100 10000
```

The documentation and contract must continue to distinguish:

* validated annihilation physics from application-supplied environmental parameterization;
* the recommended transport-coupled integration path from the legacy explicit source-generator path;
* current capabilities from deferred microscopic or environment-specific modeling.

Future implementation milestones must preserve the validated public responsibility boundary and must not silently expand the installed API or alter physics semantics.

---

## 46. One-Sentence Product Contract

> PsSource is a modular Geant4 physics extension that can be registered in an existing application to replace terminal at-rest positron annihilation with configurable, validated positronium final-state modeling while leaving the host source, geometry, transport, scoring, detector response, analysis, and output infrastructure unchanged.

PET is the principal initial application and validation domain, but PsSource is intended for arbitrary Geant4 applications involving transported positrons.

