#ifndef PS_SOURCE_ANNIHILATION_CONFIG_HH
#define PS_SOURCE_ANNIHILATION_CONFIG_HH

#include "ConfigurablePsModel.hh"
#include "PsPhysicsModel.hh"

struct PsSourceAnnihilationConfig {
    PsEnvironment environment;

    ConfigurablePsModel::DelayMode delay_mode =
        ConfigurablePsModel::DelayMode::Exponential;

    double fixed_delay_ns = 3.0;

    bool enable_three_gamma = true;

    ConfigurablePsModel::ThreeGammaModel three_gamma_model =
        ConfigurablePsModel::ThreeGammaModel::ApproximatePhaseSpace;
};

#endif
