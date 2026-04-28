#include "PositroniumGenerator.hh"
#include "PositroniumTruthInfo.hh"
#include "TimedEventModel.hh"

#include "G4Event.hh"
#include "G4Exception.hh"
#include "G4ExceptionSeverity.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleTable.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "Randomize.hh"

#include <cmath>
#include <limits>
#include <string>

namespace {
constexpr double kPi = 3.14159265358979323846;

PositroniumTruthInfo::DelayMode ConvertDelayMode(PositroniumProvider::DelayMode mode)
{
    switch (mode) {
        case PositroniumProvider::DelayMode::Fixed:
            return PositroniumTruthInfo::DelayMode::Fixed;
        case PositroniumProvider::DelayMode::Exponential:
            return PositroniumTruthInfo::DelayMode::Exponential;
        default:
            return PositroniumTruthInfo::DelayMode::Unknown;
    }
}
} // namespace

PositroniumGenerator::PositroniumGenerator()
    : G4VUserPrimaryGeneratorAction()
{
    SyncProviderConfiguration();
}

void PositroniumGenerator::SetSourcePosition(std::array<double, 3> pos_mm)
{
    m_source_position_mm = pos_mm;
    m_provider.SetSourcePosition(pos_mm);
}

void PositroniumGenerator::SetPositronKineticEnergyMeV(double e_mev)
{
    ValidatePositiveEnergy(e_mev, "Positron kinetic energy");
    m_positron_kinetic_energy_MeV = e_mev;
}

void PositroniumGenerator::SetPositronKineticEnergyKeV(double e_kev)
{
    ValidatePositiveEnergy(e_kev, "Positron kinetic energy");
    m_positron_kinetic_energy_MeV = e_kev * 1.0e-3;
}

void PositroniumGenerator::SetHasPromptGamma(bool v)
{
    m_has_prompt_gamma = v;
    m_provider.SetHasPromptGamma(v);
}

void PositroniumGenerator::SetPromptEnergyMeV(double e_mev)
{
    ValidatePositiveEnergy(e_mev, "Prompt gamma energy");
    m_prompt_energy_MeV = e_mev;
    m_provider.SetPromptEnergyMeV(e_mev);
}

void PositroniumGenerator::SetEnableQuantumEntanglement(bool v)
{
    m_enable_quantum_entanglement = v;
    m_provider.SetEnableQuantumEntanglement(v);
}

void PositroniumGenerator::SetFractions(double f_direct, double f_pps, double f_ops)
{
    m_requested_f_direct = f_direct;
    m_requested_f_pps = f_pps;
    m_requested_f_ops = f_ops;
    m_provider.SetFractions(f_direct, f_pps, f_ops);
}

void PositroniumGenerator::SetDelayMode(PositroniumProvider::DelayMode mode)
{
    m_requested_delay_mode = mode;
    m_provider.SetDelayMode(mode);
}

void PositroniumGenerator::SetTauParaPsNs(double tau_ns)
{
    m_requested_tau_pps_ns = tau_ns;
    m_provider.SetTauParaPsNs(tau_ns);
}

void PositroniumGenerator::SetTauOpsNs(double tau_ns)
{
    m_requested_tau_ops_ns = tau_ns;
    m_provider.SetTauOpsNs(tau_ns);
}

void PositroniumGenerator::SetFixedDelayNs(double fixed_ns)
{
    m_requested_fixed_delay_ns = fixed_ns;
    m_provider.SetFixedDelayNs(fixed_ns);
}

void PositroniumGenerator::SetEnableThreeGamma(bool v)
{
    m_requested_enable_three_gamma = v;
    m_provider.SetEnableThreeGamma(v);
}

void PositroniumGenerator::SetOrthoThreeGammaFraction(double f)
{
    m_requested_ortho_three_gamma_fraction = f;
    m_provider.SetOrthoThreeGammaFraction(f);
}

void PositroniumGenerator::SetEnablePositronRange(bool v)
{
    m_requested_enable_positron_range = v;
    m_provider.SetEnablePositronRange(v);
}

void PositroniumGenerator::SetPositronRangeSigmaMm(double sigma_mm)
{
    m_requested_positron_range_sigma_mm = sigma_mm;
    m_provider.SetPositronRangeSigmaMm(sigma_mm);
}

void PositroniumGenerator::ValidatePositiveEnergy(double e, const char* label) const
{
    if (e <= 0.0) {
        G4ExceptionDescription msg;
        msg << label << " must be strictly positive. Got " << e;
        G4Exception("PositroniumGenerator::ValidatePositiveEnergy",
                    "Positronium_100",
                    FatalException,
                    msg);
    }
}

std::array<double, 3> PositroniumGenerator::SampleIsotropicDirection() const
{
    const double cos_theta = -1.0 + 2.0 * G4UniformRand();
    const double sin_theta = std::sqrt(std::max(0.0, 1.0 - cos_theta * cos_theta));
    const double phi = 2.0 * kPi * G4UniformRand();

    return {
        sin_theta * std::cos(phi),
        sin_theta * std::sin(phi),
        cos_theta
    };
}

void PositroniumGenerator::SyncProviderConfiguration()
{
    m_provider.SetSourcePosition(m_source_position_mm);
    m_provider.SetHasPromptGamma(m_has_prompt_gamma);
    m_provider.SetPromptEnergyMeV(m_prompt_energy_MeV);
    m_provider.SetEnableQuantumEntanglement(m_enable_quantum_entanglement);

    m_provider.SetFractions(m_requested_f_direct, m_requested_f_pps, m_requested_f_ops);
    m_provider.SetEnableThreeGamma(m_requested_enable_three_gamma);
    m_provider.SetOrthoThreeGammaFraction(m_requested_ortho_three_gamma_fraction);
    m_provider.SetDelayMode(m_requested_delay_mode);
    m_provider.SetTauParaPsNs(m_requested_tau_pps_ns);
    m_provider.SetTauOpsNs(m_requested_tau_ops_ns);
    m_provider.SetFixedDelayNs(m_requested_fixed_delay_ns);
    m_provider.SetEnablePositronRange(m_requested_enable_positron_range);
    m_provider.SetPositronRangeSigmaMm(m_requested_positron_range_sigma_mm);
}

void PositroniumGenerator::FillRequestedTruthMetadata(PositroniumTruthInfo* truth) const
{
    if (!truth) return;

    truth->requested_f_direct = m_requested_f_direct;
    truth->requested_f_pps = m_requested_f_pps;
    truth->requested_f_ops = m_requested_f_ops;

    truth->requested_ortho_3g_fraction = m_requested_ortho_three_gamma_fraction;

    truth->requested_delay_mode = ConvertDelayMode(m_requested_delay_mode);
    truth->requested_tau_pps_ns = m_requested_tau_pps_ns;
    truth->requested_tau_ops_ns = m_requested_tau_ops_ns;
    truth->requested_fixed_delay_ns = m_requested_fixed_delay_ns;

    truth->requested_enable_positron_range = m_requested_enable_positron_range;
    truth->requested_positron_range_sigma_mm = m_requested_positron_range_sigma_mm;

    // The generator does not currently receive the native Geant4 material-side
    // ortho-Ps fraction directly from main/main_timing, so leave this explicit.
    truth->requested_native_orto_ps_fraction_valid = false;
    truth->requested_native_orto_ps_fraction = -1.0;
}

void PositroniumGenerator::GeneratePrimaries(G4Event* event)
{
    if (!event) {
        G4Exception("PositroniumGenerator::GeneratePrimaries",
                    "Positronium_101",
                    FatalException,
                    "Null G4Event pointer passed to GeneratePrimaries.");
    }

    if (m_generation_mode == GenerationMode::ExplicitProvider) {
        GeneratePrimariesExplicit(event);
    } else {
        GeneratePrimariesNative(event);
    }
}

void PositroniumGenerator::GeneratePrimariesNative(G4Event* event)
{
    const double base_time = m_use_external_base_time ? (m_base_time_ns * ns) : 0.0;

    auto* truth = new PositroniumTruthInfo();
    truth->source_event_id = ++m_generator_event_id;

    truth->generation_mode = PositroniumTruthInfo::GenerationMode::NativeGeant4;
    truth->source_tag = PositroniumTruthInfo::SourceTag::NativePositronSource;
    truth->source_is_explicit = false;

    FillRequestedTruthMetadata(truth);

    // Native mode launches a positron source and lets Geant4 determine the real
    // annihilation class / multiplicity / time / position downstream.
    truth->ps_class_id = -1;
    truth->annihilation_mode = -1;
    truth->declared_annihilation_valid = false;
    truth->delay_ns = -1.0;

    truth->has_prompt_gamma = m_has_prompt_gamma;
    truth->prompt_energy_MeV = m_has_prompt_gamma ? m_prompt_energy_MeV : 0.0;

    truth->source_x_mm = m_source_position_mm[0];
    truth->source_y_mm = m_source_position_mm[1];
    truth->source_z_mm = m_source_position_mm[2];

    truth->ann_x_mm = std::numeric_limits<double>::quiet_NaN();
    truth->ann_y_mm = std::numeric_limits<double>::quiet_NaN();
    truth->ann_z_mm = std::numeric_limits<double>::quiet_NaN();

    truth->positron_range_mm = -1.0;
    truth->medium_id = -1;
    truth->local_tau_ns = -1.0;
    truth->qe_mode = m_enable_quantum_entanglement;

    event->SetUserInformation(truth);

    auto* particle_table = G4ParticleTable::GetParticleTable();

    G4ParticleDefinition* positron_def = particle_table->FindParticle(-11); // e+
    if (!positron_def) {
        G4ExceptionDescription msg;
        msg << "Particle definition not found for positron (PDG -11).";
        G4Exception("PositroniumGenerator::GeneratePrimariesNative",
                    "Positronium_102",
                    FatalException,
                    msg);
    }

    G4ParticleDefinition* gamma_def = particle_table->FindParticle(22); // gamma
    if (m_has_prompt_gamma && !gamma_def) {
        G4ExceptionDescription msg;
        msg << "Particle definition not found for gamma (PDG 22).";
        G4Exception("PositroniumGenerator::GeneratePrimariesNative",
                    "Positronium_103",
                    FatalException,
                    msg);
    }

    const G4ThreeVector source_pos(
        m_source_position_mm[0] * mm,
        m_source_position_mm[1] * mm,
        m_source_position_mm[2] * mm
    );

    auto* source_vertex = new G4PrimaryVertex(source_pos, base_time);

    {
        auto* positron = new G4PrimaryParticle(positron_def);

        const auto dir = SampleIsotropicDirection();
        positron->SetMomentumDirection(G4ThreeVector(dir[0], dir[1], dir[2]));
        positron->SetKineticEnergy(m_positron_kinetic_energy_MeV * MeV);

        source_vertex->SetPrimary(positron);
    }

    if (m_has_prompt_gamma) {
        auto* prompt_gamma = new G4PrimaryParticle(gamma_def);

        const auto dir = SampleIsotropicDirection();
        prompt_gamma->SetMomentumDirection(G4ThreeVector(dir[0], dir[1], dir[2]));
        prompt_gamma->SetKineticEnergy(m_prompt_energy_MeV * MeV);

        source_vertex->SetPrimary(prompt_gamma);
    }

    event->AddPrimaryVertex(source_vertex);
}

void PositroniumGenerator::FillTruthFromTimedEventSpec(const TimedEventSpec& spec, G4Event* event) const
{
    auto* truth = new PositroniumTruthInfo();

    truth->source_event_id = spec.source_event_id;

    truth->generation_mode = PositroniumTruthInfo::GenerationMode::ExplicitProvider;
    truth->source_tag = PositroniumTruthInfo::SourceTag::ExplicitPhotonVertices;
    truth->source_is_explicit = true;

    FillRequestedTruthMetadata(truth);

    truth->ps_class_id = spec.ps_class_id;
    truth->annihilation_mode = spec.annihilation_mode;
    truth->delay_ns = spec.delay_ns;

    truth->has_prompt_gamma = spec.has_prompt_gamma;
    truth->prompt_energy_MeV = spec.prompt_energy_MeV;

    truth->source_x_mm = spec.source_position_mm[0];
    truth->source_y_mm = spec.source_position_mm[1];
    truth->source_z_mm = spec.source_position_mm[2];

    truth->ann_x_mm = spec.annihilation_position_mm[0];
    truth->ann_y_mm = spec.annihilation_position_mm[1];
    truth->ann_z_mm = spec.annihilation_position_mm[2];

    truth->positron_range_mm = spec.positron_range_mm;
    truth->medium_id = spec.medium_id;
    truth->local_tau_ns = spec.local_tau_ns;
    truth->qe_mode = spec.qe_mode;

    truth->declared_annihilation_valid =
        (spec.annihilation_mode > 0) &&
        std::isfinite(spec.delay_ns) &&
        std::isfinite(spec.annihilation_position_mm[0]) &&
        std::isfinite(spec.annihilation_position_mm[1]) &&
        std::isfinite(spec.annihilation_position_mm[2]);

    event->SetUserInformation(truth);
}

void PositroniumGenerator::AddExplicitVertices(const TimedEventSpec& spec,
                                               G4Event* event,
                                               double base_time) const
{
    auto* particle_table = G4ParticleTable::GetParticleTable();

    for (const auto& vtx_spec : spec.vertices) {
        const double abs_time = base_time + (vtx_spec.time_ns * ns);

        const G4ThreeVector pos(
            vtx_spec.position[0] * mm,
            vtx_spec.position[1] * mm,
            vtx_spec.position[2] * mm
        );

        auto* g4_vertex = new G4PrimaryVertex(pos, abs_time);

        for (const auto& part_spec : vtx_spec.particles) {
            G4ParticleDefinition* pdef = particle_table->FindParticle(part_spec.pdg_code);

            if (!pdef) {
                G4ExceptionDescription msg;
                msg << "Particle definition not found for PDG code: "
                    << part_spec.pdg_code;
                G4Exception("PositroniumGenerator::AddExplicitVertices",
                            "Positronium_104",
                            FatalException,
                            msg);
            }

            auto* g4_particle = new G4PrimaryParticle(pdef);

            const G4ThreeVector dir(
                part_spec.direction[0],
                part_spec.direction[1],
                part_spec.direction[2]
            );

            g4_particle->SetMomentumDirection(dir);
            g4_particle->SetKineticEnergy(part_spec.kinetic_energy_MeV * MeV);

            g4_vertex->SetPrimary(g4_particle);
        }

        event->AddPrimaryVertex(g4_vertex);
    }
}

void PositroniumGenerator::GeneratePrimariesExplicit(G4Event* event)
{
    SyncProviderConfiguration();

    const double base_time = m_use_external_base_time ? (m_base_time_ns * ns) : 0.0;

    TimedEventSpec truth_spec = m_provider.SampleNextEvent();
    truth_spec.source_event_id = ++m_generator_event_id;

    FillTruthFromTimedEventSpec(truth_spec, event);
    AddExplicitVertices(truth_spec, event, base_time);
}
