#ifndef CONFIGURABLE_PS_MODEL_HH
#define CONFIGURABLE_PS_MODEL_HH

#include "FixedParameterizedPsModel.hh"
#include "OrePowellPsModel.hh"
#include "PsPhysicsModel.hh"

#include <string>

class ConfigurablePsModel : public IPsPhysicsModel {
public:
    enum class ThreeGammaModel {
        ApproximatePhaseSpace,
        Geant4OrePowell,
        Geant4PolarizedOrePowell
    };

    using DelayMode =
        FixedParameterizedPsModel::DelayMode;

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
