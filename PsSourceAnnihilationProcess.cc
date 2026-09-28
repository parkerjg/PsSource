#include "PsSourceAnnihilationProcess.hh"
#include "PsTerminalStateBuilder.hh"

#include "G4DynamicParticle.hh"
#include "G4EmParameters.hh"
#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4Exception.hh"
#include "G4Gamma.hh"
#include "G4IonisParamMat.hh"
#include "G4Material.hh"
#include "G4ParticleChangeForGamma.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Track.hh"
#include "G4VParticleChange.hh"
#include "G4Version.hh"

#include <cstdint>
#include <exception>
#include <string>

namespace {

G4int CurrentEventId()
{
    G4EventManager* event_manager =
        G4EventManager::GetEventManager();

    if (!event_manager) {
        return -1;
    }

    const G4Event* event =
        event_manager->GetConstCurrentEvent();

    return event ? event->GetEventID() : -1;
}

}  // namespace

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

void PsSourceAnnihilationProcess::
CheckNativeConfiguration() const
{
    // With Geant4Native 2-gamma generation, the native at-rest process is
    // called for the 2-gamma branches. If the native Ore-Powell at-rest
    // model is active and any material has a non-zero ortho-positronium
    // fraction, the native process would itself convert some of those
    // events to 3-gamma, double-counting the o-Ps 3-gamma branch that
    // PsSource already selects from the PsEnvironment.
    if (
        m_config.two_gamma_model !=
        PsSourceTwoGammaModel::Geant4Native
    ) {
        return;
    }

    const G4PositronAtRestModelType at_rest_type =
        G4EmParameters::Instance()->
        PositronAtRestModelType();

    if (
        at_rest_type != fOrePowell &&
        at_rest_type != fOrePowellPolar
    ) {
        return;
    }

    for (
        const G4Material* material :
        *G4Material::GetMaterialTable()
    ) {
        const G4double ortho_fraction =
            material->GetIonisation()->
            GetOrtoPositroniumFraction();

        if (ortho_fraction > 0.0) {
            G4ExceptionDescription message;

            message
                << "Material '"
                << material->GetName()
                << "' has a native Geant4 ortho-positronium fraction of "
                << ortho_fraction
                << " while the native Ore-Powell positron at-rest model "
                << "is active. PsSource selects the o-Ps 3-gamma branch "
                << "itself, so the native fraction would double-count it. "
                << "Set the material's ortho-positronium fraction to 0, "
                << "or select a non-Ore-Powell positron at-rest model type "
                << "in G4EmParameters.";

            G4Exception(
                "PsSourceAnnihilationProcess::CheckNativeConfiguration",
                "PsSource_AnnihilationProcess_003",
                FatalException,
                message
            );
        }
    }
}

G4VParticleChange*
PsSourceAnnihilationProcess::AtRestDoIt(
    const G4Track& track,
    const G4Step& step
)
{
    if (!m_native_configuration_checked) {
        CheckNativeConfiguration();
        m_native_configuration_checked = true;
    }

    const G4int event_id =
        CurrentEventId();

    PsEnvironment environment;
    PsBranchResult branch;

    try {
        const PsTerminalState terminal_state =
            PsTerminalStateBuilder::Build(
                track,
                step,
                event_id < 0
                    ? 0
                    : static_cast<std::uint64_t>(event_id)
            );

        environment =
            m_config.environment_provider
                ? m_config.environment_provider->
                    ResolveEnvironment(terminal_state)
                : m_config.environment;

        branch =
            m_model.SampleBranch(
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

    const G4double photon_birth_time =
        track.GetGlobalTime() +
        branch.delay_ns * ns;

    const bool use_native_two_gamma =
        branch.annihilation_mode == 2 &&
        m_config.two_gamma_model ==
            PsSourceTwoGammaModel::Geant4Native;

    G4VParticleChange* particle_change = nullptr;

    std::string model_name;
    std::string model_version;
    std::string validation_status;

    if (use_native_two_gamma) {
        // Native Geant4 generates the photons, including any configured
        // entanglement, polarization, and acolinearity. PsSource only moves
        // the emission time from the positron terminal time to the
        // positronium annihilation time. The entanglement clipboard refers
        // to the secondary tracks themselves, so it is unaffected.
        particle_change =
            G4eplusAnnihilation::AtRestDoIt(
                track,
                step
            );

        const G4int secondary_count =
            particle_change->GetNumberOfSecondaries();

        for (
            G4int index = 0;
            index < secondary_count;
            ++index
        ) {
            particle_change->GetSecondary(index)->
                SetGlobalTime(photon_birth_time);
        }

        model_name = "Geant4NativeAtRest2g";
        model_version = G4Version;
        validation_status = "geant4-native-2g";
    }
    else {
        PsModelResult model_result;

        try {
            model_result =
                m_model.SampleFromBranch(
                    branch,
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

        auto* own_particle_change =
            GetParticleChange();

        own_particle_change->Initialize(track);

        own_particle_change->SetNumberOfSecondaries(
            static_cast<G4int>(
                model_result.photons.size()
            )
        );

        own_particle_change->SetProposedKineticEnergy(
            0.0
        );

        own_particle_change->ProposeLocalEnergyDeposit(
            0.0
        );

        own_particle_change->ProposeTrackStatus(
            fStopAndKill
        );

        particle_change =
            own_particle_change;

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

            gamma_track->SetTouchableHandle(
                track.GetTouchableHandle()
            );

            particle_change->
                G4VParticleChange::AddSecondary(
                    gamma_track
                );
        }

        model_name = model_result.model_name;
        model_version = model_result.model_version;
        validation_status = model_result.validation_status;
    }

    if (m_config.observer) {
        double local_tau_ns = 0.0;

        switch (branch.ps_class) {
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

        record.event_id =
            event_id;

        record.positron_track_id =
            track.GetTrackID();

        record.ps_class =
            branch.ps_class;

        record.annihilation_mode =
            branch.annihilation_mode;

        record.positron_terminal_time_ns =
            track.GetGlobalTime() / ns;

        record.sampled_ps_delay_ns =
            branch.delay_ns;

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
            model_name;

        record.model_version =
            model_version;

        record.validation_status =
            validation_status;

        m_config.observer->OnPsSourceAnnihilation(
            record
        );
    }

    return particle_change;
}
