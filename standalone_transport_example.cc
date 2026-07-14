#include "PsSourceAnnihilationPhysics.hh"

#include "G4Box.hh"
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

class StandaloneDetector
    : public G4VUserDetectorConstruction {
public:
    G4VPhysicalVolume* Construct() override
    {
        auto* material =
            G4NistManager::Instance()
                ->FindOrBuildMaterial("G4_AIR");

        const G4double half_length = 1.0 * m;

        auto* solid =
            new G4Box(
                "World",
                half_length,
                half_length,
                half_length
            );

        auto* logical =
            new G4LogicalVolume(
                solid,
                material,
                "World"
            );

        return new G4PVPlacement(
            nullptr,
            G4ThreeVector(),
            logical,
            "World",
            nullptr,
            false,
            0,
            true
        );
    }
};

class StandalonePrimaryGenerator
    : public G4VUserPrimaryGeneratorAction {
public:
    StandalonePrimaryGenerator()
        : m_gun(1)
    {
        m_gun.SetParticleDefinition(
            G4Positron::PositronDefinition()
        );

        m_gun.SetParticlePosition(
            G4ThreeVector()
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
        m_gun.GeneratePrimaryVertex(event);
    }

private:
    G4ParticleGun m_gun;
};

class StandaloneActionInitialization
    : public G4VUserActionInitialization {
public:
    void Build() const override
    {
        SetUserAction(
            new StandalonePrimaryGenerator()
        );
    }
};

int main()
{
    auto* run_manager =
        G4RunManagerFactory::CreateRunManager(
            G4RunManagerType::Serial
        );

    run_manager->SetUserInitialization(
        new StandaloneDetector()
    );

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

    PsSourceAnnihilationConfig config;

    config.environment.f_direct = 1.0;
    config.environment.f_pps = 0.0;
    config.environment.f_ops = 0.0;

    config.environment.tau_direct_ns = 0.0;
    config.environment.tau_pps_ns = 0.125;
    config.environment.tau_ops_ns = 3.0;

    config.environment.ops_2g_fraction = 1.0;
    config.environment.ops_3g_fraction = 0.0;

    config.delay_mode =
        PsSourceDelayMode::Fixed;

    config.fixed_delay_ns = 0.0;
    config.enable_three_gamma = false;

    physics->RegisterPhysics(
        new PsSourceAnnihilationPhysics(config)
    );

    run_manager->SetUserInitialization(physics);

    run_manager->SetUserInitialization(
        new StandaloneActionInitialization()
    );

    run_manager->Initialize();

    G4cout
        << "[StandaloneExample] Running 10 ordinary "
        << "transported positrons."
        << G4endl;

    run_manager->BeamOn(10);

    G4cout
        << "[StandaloneExample] PASS: run completed "
        << "without PsSource generator, truth, event action, "
        << "or tracking action."
        << G4endl;

    delete run_manager;
    return 0;
}
