#ifndef POSITRONIUM_GENERATOR_HH
#define POSITRONIUM_GENERATOR_HH

#include "G4VUserPrimaryGeneratorAction.hh"
#include "PositroniumProvider.hh"

#include <array>
#include <cstdint>

class G4Event;
class PositroniumTruthInfo;
struct TimedEventSpec;

class PositroniumGenerator : public G4VUserPrimaryGeneratorAction {
public:
    enum class GenerationMode {
        NativeGeant4,
        ExplicitProvider
    };

    PositroniumGenerator();
    virtual ~PositroniumGenerator() = default;

    virtual void GeneratePrimaries(G4Event* event) override;

    // -------------------------------------------------------------------------
    // Generation mode
    // -------------------------------------------------------------------------
    void SetGenerationMode(GenerationMode mode) { m_generation_mode = mode; }
    GenerationMode GetGenerationMode() const { return m_generation_mode; }

    // -------------------------------------------------------------------------
    // Shared source controls
    // -------------------------------------------------------------------------
    void SetSourcePosition(std::array<double, 3> pos_mm);

    void SetPositronKineticEnergyMeV(double e_mev);
    void SetPositronKineticEnergyKeV(double e_kev);

    void SetHasPromptGamma(bool v);
    void SetPromptEnergyMeV(double e_mev);

    void SetEnableQuantumEntanglement(bool v);

    void SetBaseTimeNs(double t_ns) { m_base_time_ns = t_ns; }
    void SetUseExternalBaseTime(bool v) { m_use_external_base_time = v; }

    // -------------------------------------------------------------------------
    // Explicit-provider controls
    // -------------------------------------------------------------------------
    void SetThreeGammaModel(
        PositroniumProvider::ThreeGammaModel model
    );
    void SetFractions(double f_direct, double f_pps, double f_ops);
    void SetDelayMode(PositroniumProvider::DelayMode mode);
    void SetTauParaPsNs(double tau_ns);
    void SetTauOpsNs(double tau_ns);
    void SetFixedDelayNs(double fixed_ns);

    void SetEnableThreeGamma(bool v);
    void SetOrthoThreeGammaFraction(double f);

    void SetEnablePositronRange(bool v);
    void SetPositronRangeSigmaMm(double sigma_mm);

private:
    void GeneratePrimariesNative(G4Event* event);
    void GeneratePrimariesExplicit(G4Event* event);

    void FillRequestedTruthMetadata(PositroniumTruthInfo* truth) const;
    void FillTruthFromTimedEventSpec(const TimedEventSpec& spec, G4Event* event) const;
    void AddExplicitVertices(const TimedEventSpec& spec, G4Event* event, double base_time) const;

    std::array<double, 3> SampleIsotropicDirection() const;
    void ValidatePositiveEnergy(double e_mev, const char* label) const;
    void SyncProviderConfiguration();

private:
    GenerationMode m_generation_mode = GenerationMode::NativeGeant4;

    uint64_t m_generator_event_id = 0;

    std::array<double, 3> m_source_position_mm = {0.0, 0.0, 0.0};

    // Near-rest positron default to mirror the old GPS macro behavior
    double m_positron_kinetic_energy_MeV = 1.0e-7; // 0.0001 keV

    bool   m_has_prompt_gamma = false;
    double m_prompt_energy_MeV = 1.274;

    bool m_enable_quantum_entanglement = true;

    bool m_use_external_base_time = false;
    double m_base_time_ns = 0.0;

    // -------------------------------------------------------------------------
    // Local mirrors of requested explicit-provider configuration
    // These exist so PositroniumTruthInfo can record exactly what was requested.
    // -------------------------------------------------------------------------
    double m_requested_f_direct = 0.3;
    double m_requested_f_pps = 0.2;
    double m_requested_f_ops = 0.5;

    bool   m_requested_enable_three_gamma = true;
    double m_requested_ortho_three_gamma_fraction = 1.0;

    PositroniumProvider::DelayMode m_requested_delay_mode =
        PositroniumProvider::DelayMode::Exponential;

    double m_requested_tau_pps_ns = 0.125;
    double m_requested_tau_ops_ns = 3.0;
    double m_requested_fixed_delay_ns = 3.0;

    bool   m_requested_enable_positron_range = false;
    double m_requested_positron_range_sigma_mm = 1.0;

    PositroniumProvider m_provider;
};

#endif
