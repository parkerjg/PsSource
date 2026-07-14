#ifndef FIXED_PARAMETERIZED_PS_MODEL_HH
#define FIXED_PARAMETERIZED_PS_MODEL_HH

#include "PsPhysicsModel.hh"

#include <array>
#include <utility>

class FixedParameterizedPsModel : public IPsPhysicsModel {
public:
    using DelayMode = PsSourceDelayMode;

    FixedParameterizedPsModel();

    void SetDelayMode(DelayMode mode);
    void SetFixedDelayNs(double fixed_delay_ns);

    void SetEnableThreeGamma(bool enabled);

    PsModelResult Sample(
        const PsEnvironment& environment
    ) const override;

    PsBranchResult SampleBranch(
        const PsEnvironment& environment
    ) const;

    std::vector<PsPhoton> SamplePhotons(
        PsClass ps_class
    ) const;

    std::string Name() const override;
    std::string Version() const override;
    std::string ValidationStatus() const override;

private:
    void ValidateEnvironment(
        const PsEnvironment& environment
    ) const;

    PsClass SamplePsClass(
        const PsEnvironment& environment
    ) const;

    double SampleDelayNs(
        PsClass ps_class,
        const PsEnvironment& environment
    ) const;

    std::array<double, 3> SampleIsotropicDirection() const;

    std::pair<
        std::array<double, 3>,
        std::array<double, 3>
    > SampleBackToBackDirections() const;

    void SampleThreeGammaKinematics(
        std::array<double, 3>& energies_MeV,
        std::array<std::array<double, 3>, 3>& directions
    ) const;

    double SampleUniformOpen() const;

private:
    DelayMode m_delay_mode = DelayMode::Exponential;
    double m_fixed_delay_ns = 3.0;
    bool m_enable_three_gamma = true;
};

#endif
