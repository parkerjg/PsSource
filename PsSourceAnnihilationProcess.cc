#include "PsSourceAnnihilationProcess.hh"
#include "PsTerminalStateBuilder.hh"

#include "G4DynamicParticle.hh"
#include "G4Exception.hh"
#include "G4Gamma.hh"
#include "G4ParticleChangeForGamma.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Track.hh"
#include "G4VParticleChange.hh"

#include <exception>

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
    const G4Step& step
)
{
    PsEnvironment environment;
    PsModelResult model_result;

    try {
        const PsTerminalState terminal_state =
            PsTerminalStateBuilder::Build(
                track,
                step,
                0
            );

        environment =
            m_config.environment_provider
                ? m_config.environment_provider->
                    ResolveEnvironment(terminal_state)
                : m_config.environment;

        model_result =
            m_model.Sample(
                environment
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

    if (m_config.observer) {
        double local_tau_ns = 0.0;

        switch (model_result.ps_class) {
            case PsClass::Direct2g:
                local_tau_ns =
                    environment.tau_direct_ns;
                break;

            case PsClass::ParaPs2g:
                local_tau_ns =
                    environment.tau_pps_ns;
                break;

            case PsClass::OrthoPs2g:
            case PsClass::OrthoPs3g:
                local_tau_ns =
                    environment.tau_ops_ns;
                break;
        }

        const G4ThreeVector& annihilation_position =
            track.GetPosition();

        PsSourceAnnihilationRecord record;

        record.ps_class =
            model_result.ps_class;

        record.annihilation_mode =
            model_result.annihilation_mode;

        record.positron_terminal_time_ns =
            track.GetGlobalTime() / ns;

        record.sampled_ps_delay_ns =
            model_result.delay_ns;

        record.annihilation_time_ns =
            photon_birth_time / ns;

        record.annihilation_position_mm = {
            annihilation_position.x() / mm,
            annihilation_position.y() / mm,
            annihilation_position.z() / mm
        };

        record.medium_id =
            environment.medium_id;

        record.local_tau_ns =
            local_tau_ns;

        record.model_name =
            model_result.model_name;

        record.model_version =
            model_result.model_version;

        record.validation_status =
            model_result.validation_status;

        m_config.observer->OnPsSourceAnnihilation(
            record
        );
    }

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
