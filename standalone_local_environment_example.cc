#include "PsSourceEnvironmentProvider.hh"
#include "PsSourceAnnihilationPhysics.hh"
#include "PsSourceAnnihilationObserver.hh"

#include "G4Box.hh"
#include "G4Event.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4ParticleGun.hh"
#include "G4PhysListFactory.hh"
#include "G4Positron.hh"
#include "G4RunManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4VModularPhysicsList.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VUserActionInitialization.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4ios.hh"

class CopyNumberEnvironmentProvider
    : public IPsSourceEnvironmentProvider {
public:
    CopyNumberEnvironmentProvider(
        int selected_copy_number,
        const PsEnvironment& selected_environment,
        const PsEnvironment& default_environment
    )
        : m_selected_copy_number(selected_copy_number),
          m_selected_environment(selected_environment),
          m_default_environment(default_environment)
    {
    }

    PsEnvironment ResolveEnvironment(
        const PsTerminalState& terminal_state
    ) const override
    {
        return (
            terminal_state.copy_number ==
            m_selected_copy_number
        )
            ? m_selected_environment
            : m_default_environment;
    }

private:
    int m_selected_copy_number = -1;
    PsEnvironment m_selected_environment;
    PsEnvironment m_default_environment;
};

class LocalEnvironmentDetector
    : public G4VUserDetectorConstruction {
public:
    G4VPhysicalVolume* Construct() override
    {
        auto* material =
            G4NistManager::Instance()
                ->FindOrBuildMaterial("G4_AIR");

        auto* world_solid =
            new G4Box(
                "World",
                1.0 * m,
                1.0 * m,
                1.0 * m
            );

        auto* world_logical =
            new G4LogicalVolume(
                world_solid,
                material,
                "World"
            );

        auto* world_physical =
            new G4PVPlacement(
                nullptr,
                G4ThreeVector(),
                world_logical,
                "World",
                nullptr,
                false,
                0,
                true
            );

        auto* local_solid =
            new G4Box(
                "LocalVolume",
                10.0 * cm,
                10.0 * cm,
                10.0 * cm
            );

        auto* local_logical =
            new G4LogicalVolume(
                local_solid,
                material,
                "LocalVolume"
            );

        new G4PVPlacement(
            nullptr,
            G4ThreeVector(-20.0 * cm, 0.0, 0.0),
            local_logical,
            "LocalVolume0",
            world_logical,
            false,
            0,
            true
        );

        new G4PVPlacement(
            nullptr,
            G4ThreeVector(20.0 * cm, 0.0, 0.0),
            local_logical,
            "LocalVolume1",
            world_logical,
            false,
            1,
            true
        );

        return world_physical;
    }
};

class LocalEnvironmentPrimaryGenerator
    : public G4VUserPrimaryGeneratorAction {
public:
    LocalEnvironmentPrimaryGenerator()
        : m_gun(1)
    {
        m_gun.SetParticleDefinition(
            G4Positron::PositronDefinition()
        );

        m_gun.SetParticleMomentumDirection(
            G4ThreeVector(0.0, 0.0, 1.0)
        );

        m_gun.SetParticleEnergy(
            0.0001 * keV
        );
    }

    void GeneratePrimaries(
        G4Event* event
    ) override
    {
        const G4double x =
            event->GetEventID() % 2 == 0
                ? -20.0 * cm
                : 20.0 * cm;

        m_gun.SetParticlePosition(
            G4ThreeVector(x, 0.0, 0.0)
        );

        m_gun.GeneratePrimaryVertex(event);
    }

private:
    G4ParticleGun m_gun;
};

class LocalEnvironmentActionInitialization
    : public G4VUserActionInitialization {
public:
    void Build() const override
    {
        SetUserAction(
            new LocalEnvironmentPrimaryGenerator()
        );
    }
};

class LocalEnvironmentObserver
    : public IPsSourceAnnihilationObserver {
public:
    void OnPsSourceAnnihilation(
        const PsSourceAnnihilationRecord& record
    ) override
    {
        if (
            record.ps_class == PsClass::Direct2g &&
            record.annihilation_mode == 2 &&
            record.medium_id == 100
        ) {
            ++m_direct_count;
            return;
        }

        if (
            record.ps_class == PsClass::OrthoPs3g &&
            record.annihilation_mode == 3 &&
            record.medium_id == 200
        ) {
            ++m_ortho_three_gamma_count;
            return;
        }

        ++m_unexpected_count;
    }

    int DirectCount() const
    {
        return m_direct_count;
    }

    int OrthoThreeGammaCount() const
    {
        return m_ortho_three_gamma_count;
    }

    int UnexpectedCount() const
    {
        return m_unexpected_count;
    }

private:
    int m_direct_count = 0;
    int m_ortho_three_gamma_count = 0;
    int m_unexpected_count = 0;
};

int main()
{
    auto* run_manager =
        G4RunManagerFactory::CreateRunManager(
            G4RunManagerType::Serial
        );

    run_manager->SetUserInitialization(
        new LocalEnvironmentDetector()
    );

    PsEnvironment direct_environment;
    direct_environment.medium_id = 100;
    direct_environment.f_direct = 1.0;
    direct_environment.f_pps = 0.0;
    direct_environment.f_ops = 0.0;
    direct_environment.tau_direct_ns = 0.0;
    direct_environment.tau_pps_ns = 0.125;
    direct_environment.tau_ops_ns = 3.0;
    direct_environment.ops_2g_fraction = 1.0;
    direct_environment.ops_3g_fraction = 0.0;

    PsEnvironment ortho_environment;
    ortho_environment.medium_id = 200;
    ortho_environment.f_direct = 0.0;
    ortho_environment.f_pps = 0.0;
    ortho_environment.f_ops = 1.0;
    ortho_environment.tau_direct_ns = 0.0;
    ortho_environment.tau_pps_ns = 0.125;
    ortho_environment.tau_ops_ns = 3.0;
    ortho_environment.ops_2g_fraction = 0.0;
    ortho_environment.ops_3g_fraction = 1.0;

    CopyNumberEnvironmentProvider
        environment_provider(
            1,
            ortho_environment,
            direct_environment
        );

    LocalEnvironmentObserver observer;

    PsSourceAnnihilationConfig config;
    config.environment_provider =
        &environment_provider;

    config.delay_mode =
        PsSourceDelayMode::Fixed;

    config.fixed_delay_ns = 0.0;
    config.enable_three_gamma = true;

    config.three_gamma_model =
        PsSourceThreeGammaModel::
            ApproximatePhaseSpace;

    config.observer = &observer;

    G4PhysListFactory factory;

    G4VModularPhysicsList* physics =
        factory.GetReferencePhysList(
            "FTFP_BERT"
        );

    if (!physics) {
        G4cerr
            << "Failed to create FTFP_BERT."
            << G4endl;

        delete run_manager;
        return 1;
    }

    physics->RegisterPhysics(
        new PsSourceAnnihilationPhysics(config)
    );

    run_manager->SetUserInitialization(physics);

    run_manager->SetUserInitialization(
        new LocalEnvironmentActionInitialization()
    );

    run_manager->Initialize();
    run_manager->BeamOn(10);

    const bool passed =
        observer.DirectCount() == 5 &&
        observer.OrthoThreeGammaCount() == 5 &&
        observer.UnexpectedCount() == 0;

    if (!passed) {
        G4cerr
            << "[LocalEnvironmentExample] FAIL: direct="
            << observer.DirectCount()
            << ", ortho3g="
            << observer.OrthoThreeGammaCount()
            << ", unexpected="
            << observer.UnexpectedCount()
            << G4endl;

        delete run_manager;
        return 1;
    }

    G4cout
        << "[LocalEnvironmentExample] PASS: copy number 0 "
        << "resolved to five direct 2-gamma events and copy "
        << "number 1 resolved to five ortho-Ps 3-gamma events."
        << G4endl;

    delete run_manager;
    return 0;
}
