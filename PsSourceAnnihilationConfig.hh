#ifndef PS_SOURCE_ANNIHILATION_CONFIG_HH
#define PS_SOURCE_ANNIHILATION_CONFIG_HH

#include "ConfigurablePsModel.hh"
#include "PsPhysicsModel.hh"
#include "PsSourceAnnihilationObserver.hh"
#include "PsSourceEnvironmentProvider.hh"

struct PsSourceAnnihilationConfig {
    // Default fixed environment. Used when environment_provider is null.
    PsEnvironment environment;

    // Optional, non-owning provider for resolving the local environment
    // from the transported terminal positron state.
    //
    // The caller must ensure that the provider remains alive while the
    // Geant4 run manager uses this physics.
    const IPsSourceEnvironmentProvider* environment_provider = nullptr;

    ConfigurablePsModel::DelayMode delay_mode =
        ConfigurablePsModel::DelayMode::Exponential;

    double fixed_delay_ns = 3.0;

    bool enable_three_gamma = true;

    ConfigurablePsModel::ThreeGammaModel three_gamma_model =
        ConfigurablePsModel::ThreeGammaModel::ApproximatePhaseSpace;

    // Optional, non-owning observer. The caller must ensure that the
    // observer remains alive while the Geant4 run manager uses this physics.
    IPsSourceAnnihilationObserver* observer = nullptr;
};

#endif
