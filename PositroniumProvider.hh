#ifndef POSITRONIUM_PROVIDER_HH
#define POSITRONIUM_PROVIDER_HH

#include "TimedEventModel.hh"

#include <array>
#include <utility>

class PositroniumProvider {
public:
    enum class DelayMode { Fixed, Exponential };

    PositroniumProvider();

    // Branching fractions across direct annihilation, para-Ps, and ortho-Ps.
    // These must sum to 1.
    void SetFractions(double f_direct, double f_pps, double f_ops);

    // Fraction of ortho-Ps events that decay through the explicit 3-gamma branch.
    // 0.0 => all o-Ps sampled as delayed 2g surrogate
    // 1.0 => all o-Ps sampled as explicit 3g
    void SetOrthoThreeGammaFraction(double f);

    // Convenience switch.
    void SetEnableThreeGamma(bool v);

    void SetDelayMode(DelayMode mode);

    // Characteristic lifetimes for explicit exponential sampling.
    void SetTauParaPsNs(double tau_ns);
    void SetTauOpsNs(double tau_ns);

    // Used only when DelayMode::Fixed for non-direct branches.
    void SetFixedDelayNs(double fixed_ns);

    void SetSourcePosition(std::array<double, 3> pos);

    // Optional source / metadata controls
    void SetHasPromptGamma(bool has_prompt);
    void SetPromptEnergyMeV(double e);
    void SetEnablePositronRange(bool v);
    void SetPositronRangeSigmaMm(double s);
    void SetEnableQuantumEntanglement(bool v);

    TimedEventSpec SampleNextEvent();

private:
    // Configuration
    DelayMode m_delay_mode;

    double m_f_direct;
    double m_f_pps;
    double m_f_ops;

    bool   m_enable_three_gamma;
    double m_ortho_three_gamma_fraction;

    double m_tau_pps_ns;
    double m_tau_ops_ns;
    double m_fixed_delay_ns;

    std::array<double, 3> m_source_pos;

    bool   m_has_prompt;
    double m_prompt_energy_MeV;

    bool   m_enable_positron_range;
    double m_positron_range_sigma_mm;

    bool   m_enable_qe;  // metadata only at provider level

    // Helpers
    void ValidateFractions(double f_direct, double f_pps, double f_ops) const;
    void ValidateUnitInterval(double x, const char* name) const;

    PsClass SamplePsClass() const;
    double SampleDelayNs(PsClass ps_class) const;

    std::array<double, 3> SampleIsotropicDirection() const;
    std::pair<std::array<double, 3>, std::array<double, 3>> SampleBackToBackDirections() const;

    void SampleThreeGammaKinematics(
        std::array<double, 3>& energies_mev,
        std::array<std::array<double, 3>, 3>& directions
    ) const;

    double SampleGaussian() const;
    double SampleUniformOpen() const;
};

#endif
