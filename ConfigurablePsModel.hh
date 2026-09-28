#ifndef CONFIGURABLE_PS_MODEL_HH
#define CONFIGURABLE_PS_MODEL_HH

#include "FixedParameterizedPsModel.hh"
#include "OrePowellPsModel.hh"
#include "PsPhysicsModel.hh"

#include <string>

class ConfigurablePsModel : public IPsPhysicsModel {
public:
    using ThreeGammaModel =
        PsSourceThreeGammaModel;

    using DelayMode =
        PsSourceDelayMode;

    ConfigurablePsModel();

    void SetThreeGammaModel(
        ThreeGammaModel model
    );

    ThreeGammaModel GetThreeGammaModel() const;

    void SetDelayMode(DelayMode mode);
    void SetFixedDelayNs(double delay_ns);
    void SetEnableThreeGamma(bool enabled);

    PsModelResult Sample(
        const PsEnvironment& environment
    ) const override;

    // Sample only the annihilation class and positronium delay.
    PsBranchResult SampleBranch(
        const PsEnvironment& environment
    ) const;

    // Generate the photon final state for an already-sampled branch.
    // Sample(environment) is equivalent to
    // SampleFromBranch(SampleBranch(environment), environment).
    PsModelResult SampleFromBranch(
        const PsBranchResult& branch,
        const PsEnvironment& environment
    ) const;

    std::string Name() const override;
    std::string Version() const override;
    std::string ValidationStatus() const override;

private:
    ThreeGammaModel m_three_gamma_model =
        ThreeGammaModel::ApproximatePhaseSpace;

    FixedParameterizedPsModel m_parameterized_model;

    OrePowellPsModel m_ore_powell_model;
    OrePowellPsModel m_polarized_ore_powell_model;
};

#endif
