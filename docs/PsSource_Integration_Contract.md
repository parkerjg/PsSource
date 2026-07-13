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
auto* physics_list =
    G4PhysListFactory().GetReferencePhysList("FTFP_BERT");

if (enable_pssource) {
    PsSourceConfig config;

    config.environment.f_direct = 0.60;
    config.environment.f_pps = 0.10;
    config.environment.f_ops = 0.30;

    config.environment.tau_pps_ns = 0.125;
    config.environment.tau_ops_ns = 3.0;

    config.environment.ortho_3g_fraction = 0.01;

    config.delay_mode =
        PsDelayMode::Exponential;

    config.three_gamma_model =
        PsThreeGammaModel::OrePowell;

    physics_list->RegisterPhysics(
        new PsSourcePhysicsConstructor(config)
    );
}

run_manager->SetUserInitialization(physics_list);
```

The exact class names may evolve before the first stable public release. The required behavior must not.

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
        new PsSourcePhysicsConstructor(config)
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

### 9.1 Environment Configuration

A conceptual environment structure is:

```cpp
struct PsEnvironment {
    double f_direct = 1.0;
    double f_pps = 0.0;
    double f_ops = 0.0;

    double tau_pps_ns = 0.125;
    double tau_ops_ns = 3.0;

    double ortho_3g_fraction = 0.0;
};
```

The environment defines the local positronium model parameters.

### 9.2 Delay Modes

```cpp
enum class PsDelayMode {
    Exponential,
    Fixed
};
```

Semantics:

* `Exponential`: sample a nonnegative delay from an exponential distribution with the configured lifetime.
* `Fixed`: apply the configured fixed delay to the realized positronium branch.

Direct-annihilation timing behavior must be documented explicitly by the implementation. The current validated transport behavior uses zero additional positronium delay for direct annihilation.

### 9.3 Three-Photon Models

```cpp
enum class PsThreeGammaModel {
    ApproximatePhaseSpace,
    OrePowell,
    PolarizedOrePowell
};
```

Conceptual meanings:

* `ApproximatePhaseSpace`: controlled approximate three-photon generation.
* `OrePowell`: validated Ore–Powell energy and angular generation.
* `PolarizedOrePowell`: Ore–Powell generation with polarization-aware photon construction.

Unavailable backends must fail clearly during initialization.

### 9.4 Top-Level Configuration

A conceptual configuration object is:

```cpp
struct PsSourceConfig {
    PsEnvironment environment;

    PsDelayMode delay_mode =
        PsDelayMode::Exponential;

    double fixed_delay_ns = 0.0;

    PsThreeGammaModel three_gamma_model =
        PsThreeGammaModel::OrePowell;

    bool enable_polarization = false;
};
```

Future configuration may include:

* environment-provider selection;
* optional truth-recorder selection;
* diagnostic verbosity;
* model-version selection;
* validation metadata;
* user-defined backend selection.

Public configuration should remain focused on quantities an external user needs.

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
* a QEPET-specific class;
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

The public design must allow environment parameters to depend on the transported positron terminal state.

A conceptual interface is:

```cpp
class IPsEnvironmentProvider {
public:
    virtual ~IPsEnvironmentProvider() = default;

    virtual PsEnvironment Resolve(
        const PsTerminalState& terminal_state
    ) const = 0;
};
```

### 14.1 Initial Provider

The initial required provider is:

```cpp
UniformPsEnvironmentProvider
```

It returns the same environment for all terminal states.

### 14.2 Future Providers

Future provider implementations may resolve environment from:

* material;
* region;
* logical volume;
* physical volume;
* copy number;
* touchable hierarchy;
* spatial coordinates;
* voxel maps;
* detector subsystem;
* target layer;
* user callbacks;
* externally supplied material-property databases.

Examples:

```cpp
MaterialPsEnvironmentProvider
RegionPsEnvironmentProvider
VolumePsEnvironmentProvider
VoxelPsEnvironmentProvider
CallbackPsEnvironmentProvider
```

### 14.3 Resolution Location

Environment resolution must occur at the transported terminal state.

It must not be based solely on the positron source location.

### 14.4 No Automatic Physical Inference

PsSource must not claim to infer material-specific, chemical, condensed-matter, biological, or tissue-specific positronium parameters directly from generic Geant4 material composition unless a separately validated model is explicitly provided.

The environment provider supplies model parameters. It does not automatically derive physical truth from generic material definitions.

---

## 15. Truth Recording Is Optional

Truth recording must never be required for physics execution.

PsSource must run successfully when the host application has:

* no custom `G4VUserEventInformation`;
* no PsSource event action;
* no PsSource tracking action;
* no CSV writer;
* no ROOT writer;
* no truth recorder.

The default behavior should be equivalent to a null recorder.

Conceptually:

```cpp
class IPsTruthRecorder {
public:
    virtual ~IPsTruthRecorder() = default;

    virtual void Record(
        const PsAnnihilationRecord& record
    ) = 0;
};
```

A null implementation may be used:

```cpp
class NullPsTruthRecorder final
    : public IPsTruthRecorder {
public:
    void Record(
        const PsAnnihilationRecord&
    ) override
    {
    }
};
```

Physics execution must not fail because optional truth output is absent.

---

## 16. Optional Truth Record

A public truth record should describe one realized annihilation independently of the current application.

A conceptual structure is:

```cpp
struct PsAnnihilationRecord {
    int event_id = -1;

    int positron_track_id = -1;
    int positron_parent_id = -1;

    PsClass ps_class = PsClass::Direct2g;

    int annihilation_mode = 2;
    int photon_count = 2;

    double terminal_time_ns = 0.0;
    double sampled_delay_ns = 0.0;
    double annihilation_time_ns = 0.0;

    G4ThreeVector terminal_position;
    G4ThreeVector annihilation_position;

    std::string material_name;
    std::string region_name;
    std::string logical_volume_name;
    std::string physical_volume_name;

    int copy_number = -1;

    std::string model_name;
    std::string model_version;
    std::string validation_status;
};
```

Potential additional fields may include:

* annihilation index within event;
* source-event identifier;
* positron terminal kinetic energy;
* photon energies;
* photon directions;
* photon polarizations;
* environment identifier;
* provider identifier;
* random-seed metadata;
* handoff status;
* user-defined annotations.

The public record must not require a particular file format.

CSV, ROOT, HDF5, JSON, database, or application-specific output should be implemented by optional recorder components.

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

The reusable PsSource component should become a library target.

Conceptual target:

```cmake
add_library(PsSource ...)
```

External applications should eventually be able to use:

```cmake
find_package(PsSource REQUIRED)

target_link_libraries(
    my_geant4_application
    PRIVATE
    PsSource::PsSource
)
```

The installed package should provide:

* public headers;
* compiled library;
* exported CMake target;
* package configuration;
* version information;
* minimal integration example.

Application-specific validators and regression scripts need not be installed as part of the runtime library.

---

## 33. Public Header Boundaries

Public headers should expose only stable integration concepts.

Likely public interfaces include:

```text
PsSourceConfig.hh
PsSourcePhysicsConstructor.hh
PsSourceAnnihilationProcess.hh
PsEnvironment.hh
PsEnvironmentProvider.hh
PsTerminalState.hh
PsTruthRecorder.hh
PsAnnihilationRecord.hh
PsClass.hh
PsPhysicsModel.hh
```

Application-specific headers should remain private.

Public headers must not depend on:

* current CSV schemas;
* current command-line parser;
* current detector classes;
* PET-specific classes;
* current validation scripts.

---

## 34. Ownership and Lifetime

The public API must document ownership for:

* configuration objects;
* environment providers;
* truth recorders;
* model backends;
* physics constructors;
* process instances.

Preferred patterns include:

* immutable configuration;
* `std::shared_ptr<const ...>` for shared read-only providers;
* `std::unique_ptr` for exclusive ownership;
* explicit non-owning references only when lifetime is guaranteed.

Raw-pointer ownership ambiguity should be avoided.

---

## 35. Thread Safety

The reusable implementation must be compatible with Geant4 multithreading.

The design should avoid:

* mutable global state;
* unsynchronized shared output;
* shared mutable random-number generators;
* non-thread-safe singleton recorders;
* process instances reused unsafely across workers.

Preferred behavior:

* immutable shared configuration;
* worker-local process and model state;
* Geant4 random-engine use;
* thread-local or synchronized optional truth output;
* documented reproducibility behavior.

Statistical equivalence across thread counts is required.

Bitwise-identical event ordering across thread counts is not required.

---

## 36. Randomness and Reproducibility

All stochastic sampling should use the Geant4-compatible random-number system.

The implementation should not introduce an unrelated hidden random engine.

Run metadata should allow recording:

* seed;
* random engine;
* event count;
* model version;
* configuration;
* thread count;
* Geant4 version.

Reproducibility expectations should be documented separately for:

* sequential runs;
* multithreaded runs;
* cross-version runs.

---

## 37. Versioning Policy

PsSource will follow semantic versioning after the first stable external integration release.

Format:

```text
MAJOR.MINOR.PATCH
```

### Major Version

Increment for:

* breaking public API changes;
* changed truth numeric semantics;
* removed public interfaces;
* incompatible configuration changes;
* changed default physics with external consequences.

### Minor Version

Increment for:

* backward-compatible new models;
* new optional truth fields;
* new environment providers;
* new public helper interfaces;
* new examples.

### Patch Version

Increment for:

* bug fixes;
* build fixes;
* documentation corrections;
* validation improvements that do not alter public behavior.

### Stable Numeric Semantics

After the first stable release:

* existing `PsClass` numeric values remain stable;
* existing public enum values are not renumbered;
* truth-field meaning is not silently changed.

Before version 1.0, public interfaces may evolve, but changes must be documented.

---

## 38. Supported Geant4 Versions

The initial supported Geant4 version is the version used for validated development and regression testing.

Current validated baseline:

```text
Geant4 11.3.2
```

Before a stable release, at least one additional Geant4 version should be tested.

Support documentation should distinguish:

* fully validated versions;
* build-tested versions;
* unsupported versions.

---

## 39. Non-Goals for the Initial Drop-In Release

The following are not part of the initial external integration release:

* transport of a physical Ps atom during its lifetime;
* Ps diffusion;
* Ps spatial displacement;
* in-flight Ps formation;
* formation before positron terminal rest;
* magnetic-substate-resolved control;
* automatic physical inference from generic material composition;
* automatic tissue classification;
* patient-specific parameter estimation;
* native GATE plugin;
* detector electronics;
* digitization;
* trigger logic;
* coincidence sorting;
* reconstruction;
* image analysis;
* spectroscopy analysis;
* experiment-specific event selection.

These may be addressed in future research or extension layers.

---

## 40. Scientific Limitations

The current transport-coupled model represents Ps formation and annihilation at the transported positron terminal state.

It does not currently model:

* a finite spatial path of the Ps atom;
* Ps diffusion during the sampled lifetime;
* formation while the positron is still in flight;
* chemistry-driven Ps formation mechanisms;
* material-dependent collision or quenching dynamics;
* magnetic-substate evolution;
* external-field effects unless provided by a specific future model.

Environment parameters are externally supplied model inputs.

They are not automatically inferred from Geant4 material composition.

---

## 41. Acceptance Tests

The public integration contract is considered implemented only when the following tests pass.

### 41.1 Native-Off Test

Configuration:

```text
PsSource not registered
```

Expected:

* native Geant4 annihilation remains active;
* no PsSource classes appear;
* no PsSource truth appears;
* host application runs normally.

### 41.2 PsSource-On Test

Configuration:

```text
PsSource registered
```

Expected:

* terminal at-rest annihilation uses PsSource;
* one annihilation process remains active;
* generated photons are ordinary Geant4 tracks;
* host scoring and downstream processing remain unchanged.

### 41.3 No-Truth Test

Configuration:

```text
PsSource enabled
No PsSource event information
No truth recorder
```

Expected:

* run succeeds;
* physics output remains valid;
* no fatal truth-related error occurs.

### 41.4 Truth-Enabled Test

Configuration:

```text
PsSource enabled
Optional truth recorder enabled
```

Expected:

* truth record is complete;
* physics matches the no-truth run statistically;
* recorder does not alter physics.

### 41.5 Source-Independence Test

Test with at least:

* custom primary generator;
* `G4GeneralParticleSource`;
* realistic positron spectrum, radioactive decay, or accelerator-produced positron source.

Expected:

* no source-specific PsSource dependency;
* terminal handoff remains valid.

### 41.6 Geometry-Independence Test

Test with at least two geometrically distinct host applications.

Examples may include:

* imaging geometry;
* simple material target;
* beamline or spectroscopy geometry.

Expected:

* no PsSource geometry dependency;
* generated photons interact normally.

### 41.7 Detector-Independence Test

Use at least two different scoring or detector configurations.

Expected:

* PsSource integration does not require detector changes;
* generated photons are handled through ordinary Geant4 tracking.

### 41.8 Deterministic Transport Tests

Required cases:

* direct 2γ, zero delay;
* p-Ps 2γ, fixed delay;
* o-Ps 2γ, fixed delay;
* o-Ps 3γ, fixed delay.

Validate:

* realized class;
* multiplicity;
* terminal position;
* terminal time;
* sampled delay;
* model provenance;
* energy closure;
* momentum closure.

### 41.9 Statistical Delay Tests

Required cases:

* p-Ps exponential lifetime;
* o-Ps exponential lifetime.

Validate:

* no negative delays;
* mean lifetime;
* variance or coefficient of variation;
* exponential-distribution agreement;
* event-by-event timing decomposition.

### 41.10 In-Flight Test

Expected:

* native in-flight annihilation remains available;
* Ps delay is not applied;
* no Ps class is falsely assigned;
* no duplicate terminal photons are produced.

### 41.11 Multithreading Test

Run with:

* one thread;
* two threads;
* four threads.

Validate:

* stability;
* complete event accounting;
* no duplicate truth;
* statistically consistent results.

### 41.12 Installed-Consumer Test

Procedure:

1. build PsSource;
2. install to a temporary prefix;
3. configure an external example with `find_package(PsSource)`;
4. link against `PsSource::PsSource`;
5. run native-off and PsSource-on cases.

Expected:

* no use of repository-private headers;
* no use of current application code;
* successful downstream build and run.

---

## 42. Reference External Integration Examples

The repository should include at least one minimal external-style Geant4 demonstration application.

The example should contain:

* simple geometry;
* independent positron source;
* independent scoring;
* reference physics list;
* one PsSource on/off switch.

Example commands:

```bash
./pssource_minimal_toggle \
    --pssource off \
    --events 1000
```

```bash
./pssource_minimal_toggle \
    --pssource on \
    --events 1000
```

The example must not require:

* `PositroniumGenerator`;
* `TimedEventSpec`;
* `PositroniumTruthInfo`;
* current detector geometry;
* current timing application.

A second application-domain example may later demonstrate:

* PET or QEPET integration;
* accelerator-target integration;
* positron-beam material studies;
* spectroscopy geometry.

The minimal example is the primary proof that PsSource can be integrated into an existing Geant4 application.

---

## 43. PET and QEPET Compatibility Goal

PET is an important target application but not a software requirement.

Expected PET workflow:

```text
positron or radionuclide source
        ↓
phantom transport
        ↓
PsSource terminal annihilation
        ↓
Geant4 photon transport
        ↓
detector interactions
        ↓
optical or parameterized detector response
        ↓
digitization
        ↓
coincidence processing
        ↓
reconstruction
```

PsSource does not own PET-specific:

* scanner geometry;
* scintillation;
* optical transport;
* SiPM response;
* electronics;
* coincidence processing;
* reconstruction.

For QEPET, PsSource should operate only as the annihilation-physics layer upstream of the existing detector simulation.

---

## 44. Accelerator and Fundamental-Physics Compatibility Goal

PsSource should also support non-imaging Geant4 applications.

An accelerator or positron-beam workflow may be:

```text
beam or secondary positron production
        ↓
beamline and target transport
        ↓
positron terminal state
        ↓
PsSource terminal annihilation
        ↓
Geant4 photon transport
        ↓
calorimeter, spectroscopy, or tracking detector
        ↓
experiment-specific analysis
```

PsSource does not own:

* beam generation;
* accelerator optics;
* target geometry;
* detector geometry;
* trigger logic;
* experiment-specific reconstruction;
* particle-identification logic.

The software contract is therefore broader than PET even when PET remains the principal initial scientific demonstration.

---

## 45. Documentation Requirements

The public release documentation should include:

* installation instructions;
* CMake integration example;
* native-off versus PsSource-on example;
* configuration reference;
* environment-provider description;
* truth-recorder description;
* class-ID table;
* current limitations;
* supported Geant4 versions;
* regression commands;
* minimal external example;
* model-provenance explanation.

Documentation must clearly distinguish:

* explicit event-generation mode;
* transport-coupled mode.

Transport-coupled mode should be presented as the reusable integration pathway.

PET examples should be identified as application examples rather than software requirements.

---

## 46. Contract Review Questions

Before code changes are accepted, reviewers should be able to answer the following from this document.

### Activation

* How is PsSource enabled?
* What happens when it is disabled?
* Which Geant4 process is replaced?

### Scope

* What does PsSource own?
* What does the host application retain?
* Is the software limited to PET? No.

### Handoff

* When does PsSource act?
* Which time is used?
* Which position is used?

### Independence

* Is the current generator required? No.
* Is the current detector required? No.
* Is truth required? No.
* Is the current timing application required? No.
* Is a PET geometry required? No.
* Is a radionuclide source required? No.

### Configuration

* Which parameters are public?
* Which values are invalid?
* How are fractions handled?

### Truth

* Is truth optional? Yes.
* What is the minimum record?
* Who controls output formatting? The host application or optional recorder.

### Compatibility

* Does native positron transport remain active? Yes.
* Does normal photon transport remain active? Yes.
* Does in-flight annihilation remain native? Intended yes, subject to dedicated validation.
* Are current explicit-mode tools retained? Yes.

### Limitations

* Is Ps atom transport modeled? No.
* Is in-flight Ps formation modeled? No.
* Are environment parameters inferred automatically from generic materials? No.

---

## 47. Completion Criteria for Stage 1

Stage 1 is complete when:

1. this document is committed to the repository;
2. the public responsibility boundary is accepted;
3. the transport handoff is accepted;
4. truth is explicitly defined as optional;
5. source, geometry, and detector independence are accepted;
6. PET is defined as an application domain rather than a software requirement;
7. realized class numeric semantics are frozen;
8. initial configuration concepts are accepted;
9. non-goals are accepted;
10. acceptance tests are agreed upon;
11. no physics code has changed as part of this stage.

The contract should then guide the next implementation milestone:

> Remove mandatory application-specific truth dependencies and prove that PsSource runs inside a minimal external Geant4 host application with no PsSource-specific source, detector, event-action, or truth infrastructure.

---

## 48. One-Sentence Product Contract

> PsSource is a modular Geant4 physics extension that can be registered in an existing application to replace terminal at-rest positron annihilation with configurable, validated positronium final-state modeling while leaving the host source, geometry, transport, scoring, detector response, analysis, and output infrastructure unchanged.

PET is the principal initial application and validation domain, but PsSource is intended for arbitrary Geant4 applications involving transported positrons.

