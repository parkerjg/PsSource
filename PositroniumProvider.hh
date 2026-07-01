#ifndef POSITRONIUM_PROVIDER_HH
#define POSITRONIUM_PROVIDER_HH

#include "ConfigurablePsModel.hh"
#include "TimedEventModel.hh"

#include <array>

class PositroniumProvider {
public:

    using DelayMode = ConfigurablePsModel::DelayMode;
    using ThreeGammaModel = ConfigurablePsModel::ThreeGammaModel;

    PositroniumProvider();

    void SetFractions(
        double f_direct,
        double f_pps,
        double f_ops
    );

    void SetOrthoThreeGammaFraction(double fraction);
    void SetEnableThreeGamma(bool enabled);

    void SetDelayMode(DelayMode mode);
    void SetTauDirectNs(double tau_ns);
    void SetTauParaPsNs(double tau_ns);
    void SetTauOpsNs(double tau_ns);
    void SetFixedDelayNs(double fixed_delay_ns);

    void SetSourcePosition(
        std::array<double, 3> position_mm
    );

    void SetHasPromptGamma(bool enabled);
    void SetPromptEnergyMeV(double energy_MeV);

    void SetEnablePositronRange(bool enabled);
    void SetPositronRangeSigmaMm(double sigma_mm);

    void SetEnableQuantumEntanglement(bool enabled);
    void SetThreeGammaModel(ThreeGammaModel model);

    TimedEventSpec SampleNextEvent();

private:
    void ValidateFractionTriplet(
        double f_direct,
        double f_pps,
        double f_ops
    ) const;

    void ValidateUnitInterval(
        double value,
        const char* name
    ) const;

    std::array<double, 3>
    SampleIsotropicDirection() const;

    double SampleGaussian() const;
    double SampleUniformOpen() const;

private:
    PsEnvironment m_environment;
    ConfigurablePsModel m_physics_model;

    std::array<double, 3> m_source_position_mm = {
        0.0, 0.0, 0.0
    };

    bool m_has_prompt_gamma = false;
    double m_prompt_energy_MeV = 1.274;

    bool m_enable_positron_range = false;
    double m_positron_range_sigma_mm = 1.0;

    bool m_enable_quantum_entanglement = true;

};

#endif
