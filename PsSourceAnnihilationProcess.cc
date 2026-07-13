#include "PsSourceAnnihilationProcess.hh"
#include "PositroniumTruthInfo.hh"

#include "G4DynamicParticle.hh"
#include "G4Exception.hh"
#include "G4Gamma.hh"
#include "G4ParticleChangeForGamma.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Track.hh"
#include "G4VParticleChange.hh"
#include "G4Event.hh"
#include "G4EventManager.hh"

#include <exception>
#include <cmath>

PsSourceAnnihilationProcess::
PsSourceAnnihilationProcess(
    const PsSourceAnnihilationConfig& config,
    const G4String& name
)
    : G4eplusAnnihilation(name),
      m_config(config)
{
    m_model.SetDelayMode(
        m_config.delay_mode
    );

    m_model.SetFixedDelayNs(
        m_config.fixed_delay_ns
    );

    m_model.SetEnableThreeGamma(
        m_config.enable_three_gamma
    );

    m_model.SetThreeGammaModel(
        m_config.three_gamma_model
    );
}

G4VParticleChange*
PsSourceAnnihilationProcess::AtRestDoIt(
    const G4Track& track,
    const G4Step&
)
{
    PsModelResult model_result;

    try {
        model_result =
            m_model.Sample(
                m_config.environment
            );
    }
    catch (const std::exception& error) {
        G4ExceptionDescription message;

        message
            << "PsSource annihilation model failed: "
            << error.what();

        G4Exception(
            "PsSourceAnnihilationProcess::AtRestDoIt",
            "PsSource_AnnihilationProcess_001",
            FatalException,
            message
        );
    }

    if (
        model_result.photons.size() !=
        static_cast<std::size_t>(
            model_result.annihilation_mode
        )
    ) {
        G4ExceptionDescription message;

        message
            << "Annihilation mode is "
            << model_result.annihilation_mode
            << ", but the physics model returned "
            << model_result.photons.size()
            << " photons.";

        G4Exception(
            "PsSourceAnnihilationProcess::AtRestDoIt",
            "PsSource_AnnihilationProcess_002",
            FatalException,
            message
        );
    }

    auto* particle_change =
        GetParticleChange();

    particle_change->Initialize(track);

    particle_change->SetNumberOfSecondaries(
        static_cast<G4int>(
            model_result.photons.size()
        )
    );

    particle_change->SetProposedKineticEnergy(
        0.0
    );

    particle_change->ProposeLocalEnergyDeposit(
        0.0
    );

    particle_change->ProposeTrackStatus(
        fStopAndKill
    );

    const G4double photon_birth_time =
        track.GetGlobalTime() +
        model_result.delay_ns * ns;

    G4EventManager* event_manager =
        G4EventManager::GetEventManager();

    G4Event* current_event =
        event_manager
            ? event_manager->GetNonconstCurrentEvent()
            : nullptr;

    auto* truth =
        current_event
            ? dynamic_cast<PositroniumTruthInfo*>(
                  current_event->GetUserInformation()
              )
            : nullptr;

    if (!truth) {
        G4Exception(
            "PsSourceAnnihilationProcess::AtRestDoIt",
            "PsSource_AnnihilationProcess_003",
            FatalException,
            "Transport-coupled annihilation could not access "
            "PositroniumTruthInfo for the current event."
        );
    }

    truth->ps_class_id =
        static_cast<int>(
            model_result.ps_class
        );

    truth->annihilation_mode =
        model_result.annihilation_mode;

    // Total event time from the Geant4 event origin to annihilation.
    // This includes positron transport followed by the sampled Ps delay.
    truth->positron_terminal_time_ns =
        track.GetGlobalTime() / ns;

    truth->sampled_ps_delay_ns =
        model_result.delay_ns;

    truth->delay_ns =
        truth->positron_terminal_time_ns +
        truth->sampled_ps_delay_ns;

    const G4ThreeVector& annihilation_position =
        track.GetPosition();

    truth->ann_x_mm =
        annihilation_position.x() / mm;

    truth->ann_y_mm =
        annihilation_position.y() / mm;

    truth->ann_z_mm =
        annihilation_position.z() / mm;

    const double displacement_x_mm =
        truth->ann_x_mm -
        truth->source_x_mm;

    const double displacement_y_mm =
        truth->ann_y_mm -
        truth->source_y_mm;

    const double displacement_z_mm =
        truth->ann_z_mm -
        truth->source_z_mm;

    truth->positron_range_mm =
        std::sqrt(
            displacement_x_mm * displacement_x_mm +
            displacement_y_mm * displacement_y_mm +
            displacement_z_mm * displacement_z_mm
        );

    truth->medium_id =
        m_config.environment.medium_id;

    switch (model_result.ps_class) {
        case PsClass::Direct2g:
            truth->local_tau_ns =
                m_config.environment.tau_direct_ns;
            break;

        case PsClass::ParaPs2g:
            truth->local_tau_ns =
                m_config.environment.tau_pps_ns;
            break;

        case PsClass::OrthoPs2g:
        case PsClass::OrthoPs3g:
            truth->local_tau_ns =
                m_config.environment.tau_ops_ns;
            break;
    }

    truth->physics_model_name =
        model_result.model_name;

    truth->physics_model_version =
        model_result.model_version;

    truth->physics_validation_status =
        model_result.validation_status;

    truth->declared_annihilation_valid =
        true;

    auto* gamma_definition =
        G4Gamma::GammaDefinition();

    for (
        const PsPhoton& photon :
        model_result.photons
    ) {
        const G4ThreeVector direction(
            photon.direction[0],
            photon.direction[1],
            photon.direction[2]
        );

        auto* dynamic_gamma =
            new G4DynamicParticle(
                gamma_definition,
                direction,
                photon.kinetic_energy_MeV * MeV
            );

        if (photon.polarization_valid) {
            dynamic_gamma->SetPolarization(
                photon.polarization[0],
                photon.polarization[1],
                photon.polarization[2]
            );
        }

        auto* gamma_track =
            new G4Track(
                dynamic_gamma,
                photon_birth_time,
                track.GetPosition()
            );

        particle_change->
            G4VParticleChange::AddSecondary(
                gamma_track
            );
    }

    return particle_change;
}
