// Regression test for native 2-gamma delegation (PsSourceTwoGammaModel::
// Geant4Native).
//
// Transports near-rest positrons in water with every annihilation forced to
// the o-Ps pick-off 2-gamma branch and an exponential positronium delay, with
// Geant4 quantum entanglement enabled. For every event it verifies that:
//
//   1. PsSource reports exactly one annihilation, with the correct Geant4
//      event and positron track identifiers and native 2-gamma provenance;
//   2. the annihilation produces exactly two photons;
//   3. each photon is emitted at the positron terminal time plus the sampled
//      positronium delay;
//   4. each photon carries the native Geant4 entanglement information.
//
// It also checks that the mean sampled delay agrees with the configured
// o-Ps lifetime.

#include "PsSourceAnnihilationObserver.hh"
#include "PsSourceAnnihilationPhysics.hh"

#include "G4Box.hh"
#include "G4EmParameters.hh"
#include "G4Event.hh"
#include "G4Gamma.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4ParticleGun.hh"
#include "G4PhysListFactory.hh"
#include "G4PhysicsModelCatalog.hh"
#include "G4Positron.hh"
#include "G4RunManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Track.hh"
#include "G4UserEventAction.hh"
#include "G4UserTrackingAction.hh"
#include "G4VModularPhysicsList.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VProcess.hh"
#include "G4VUserActionInitialization.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4ios.hh"

#include <cmath>
#include <vector>

namespace {

constexpr int kEventCount = 2000;
constexpr double kOrthoLifetimeNs = 2.0;
constexpr double kTimeToleranceNs = 1.0e-6;

struct PhotonInfo {
    double global_time_ns = 0.0;
    bool entangled = false;
};

// Shared state for the serial run.
struct TestState {
    std::vector<PsSourceAnnihilationRecord> records;
    std::vector<PhotonInfo> photons;

    int failures = 0;
    int checked_events = 0;
    double delay_sum_ns = 0.0;
};

TestState g_state;

void Fail(
    int event_id,
    const char* reason
)
{
    ++g_state.failures;

    if (g_state.failures <= 20) {
        G4cerr
            << "[NativeTwoGammaTest] FAIL event "
            << event_id
            << ": "
            << reason
            << G4endl;
    }
}

}  // namespace

class RecordingObserver
    : public IPsSourceAnnihilationObserver {
public:
    void OnPsSourceAnnihilation(
        const PsSourceAnnihilationRecord& record
    ) override
    {
        g_state.records.push_back(record);
    }
};

class WaterDetector
    : public G4VUserDetectorConstruction {
public:
    G4VPhysicalVolume* Construct() override
    {
        auto* material =
            G4NistManager::Instance()
                ->FindOrBuildMaterial("G4_WATER");

        const G4double half_length = 10.0 * cm;

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

class RestPositronGenerator
    : public G4VUserPrimaryGeneratorAction {
public:
    RestPositronGenerator()
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

class AnnihilationPhotonTracker
    : public G4UserTrackingAction {
public:
    AnnihilationPhotonTracker()
        : m_entanglement_model_id(
              G4PhysicsModelCatalog::GetModelID(
                  "model_GammaGammaEntanglement"
              )
          )
    {
    }

    void PreUserTrackingAction(
        const G4Track* track
    ) override
    {
        if (
            track->GetDefinition() !=
            G4Gamma::GammaDefinition()
        ) {
            return;
        }

        // Annihilation photons of the primary positron.
        const G4VProcess* creator =
            track->GetCreatorProcess();

        if (
            track->GetParentID() != 1 ||
            !creator ||
            creator->GetProcessName() != "annihil"
        ) {
            return;
        }

        PhotonInfo info;

        info.global_time_ns =
            track->GetGlobalTime() / ns;

        info.entangled =
            track->GetAuxiliaryTrackInformation(
                m_entanglement_model_id
            ) != nullptr;

        g_state.photons.push_back(info);
    }

private:
    G4int m_entanglement_model_id;
};

class CheckingEventAction
    : public G4UserEventAction {
public:
    void BeginOfEventAction(
        const G4Event*
    ) override
    {
        g_state.records.clear();
        g_state.photons.clear();
    }

    void EndOfEventAction(
        const G4Event* event
    ) override
    {
        const int event_id =
            event->GetEventID();

        ++g_state.checked_events;

        if (g_state.records.size() != 1) {
            Fail(event_id, "expected exactly one PsSource record");
            return;
        }

        const PsSourceAnnihilationRecord& record =
            g_state.records.front();

        if (record.event_id != event_id) {
            Fail(event_id, "record event_id does not match event");
        }

        if (record.positron_track_id != 1) {
            Fail(event_id, "record positron_track_id is not 1");
        }

        if (
            record.ps_class != PsClass::OrthoPs2g ||
            record.annihilation_mode != 2
        ) {
            Fail(event_id, "expected an o-Ps 2-gamma annihilation");
        }

        if (record.model_name != "Geant4NativeAtRest2g") {
            Fail(event_id, "expected native 2-gamma provenance");
        }

        if (
            std::abs(
                record.annihilation_time_ns -
                record.positron_terminal_time_ns -
                record.sampled_ps_delay_ns
            ) > kTimeToleranceNs
        ) {
            Fail(event_id, "annihilation time != terminal time + delay");
        }

        if (g_state.photons.size() != 2) {
            Fail(event_id, "expected exactly two annihilation photons");
            return;
        }

        for (const PhotonInfo& photon : g_state.photons) {
            if (
                std::abs(
                    photon.global_time_ns -
                    record.annihilation_time_ns
                ) > kTimeToleranceNs
            ) {
                Fail(event_id, "photon time != annihilation time");
            }

            if (!photon.entangled) {
                Fail(event_id, "photon lacks entanglement information");
            }
        }

        g_state.delay_sum_ns +=
            record.sampled_ps_delay_ns;
    }
};

class TestActionInitialization
    : public G4VUserActionInitialization {
public:
    void Build() const override
    {
        SetUserAction(
            new RestPositronGenerator()
        );

        SetUserAction(
            new AnnihilationPhotonTracker()
        );

        SetUserAction(
            new CheckingEventAction()
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
        new WaterDetector()
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

    G4EmParameters::Instance()->
        SetQuantumEntanglement(true);

    RecordingObserver observer;

    PsSourceAnnihilationConfig config;

    config.environment.f_direct = 0.0;
    config.environment.f_pps = 0.0;
    config.environment.f_ops = 1.0;

    config.environment.tau_ops_ns = kOrthoLifetimeNs;

    config.environment.ops_2g_fraction = 1.0;
    config.environment.ops_3g_fraction = 0.0;

    config.delay_mode =
        PsSourceDelayMode::Exponential;

    config.enable_three_gamma = true;

    config.two_gamma_model =
        PsSourceTwoGammaModel::Geant4Native;

    config.observer = &observer;

    physics->RegisterPhysics(
        new PsSourceAnnihilationPhysics(config)
    );

    run_manager->SetUserInitialization(physics);

    run_manager->SetUserInitialization(
        new TestActionInitialization()
    );

    run_manager->Initialize();

    run_manager->BeamOn(kEventCount);

    if (g_state.checked_events != kEventCount) {
        G4cerr
            << "[NativeTwoGammaTest] FAIL: checked "
            << g_state.checked_events
            << " of "
            << kEventCount
            << " events."
            << G4endl;

        ++g_state.failures;
    }

    // The mean of N exponential samples has standard error tau / sqrt(N).
    const double mean_delay_ns =
        g_state.delay_sum_ns / kEventCount;

    const double standard_error_ns =
        kOrthoLifetimeNs / std::sqrt(
            static_cast<double>(kEventCount)
        );

    if (
        std::abs(mean_delay_ns - kOrthoLifetimeNs) >
        5.0 * standard_error_ns
    ) {
        G4cerr
            << "[NativeTwoGammaTest] FAIL: mean delay "
            << mean_delay_ns
            << " ns differs from tau_ops "
            << kOrthoLifetimeNs
            << " ns by more than 5 standard errors."
            << G4endl;

        ++g_state.failures;
    }

    delete run_manager;

    if (g_state.failures > 0) {
        G4cerr
            << "[NativeTwoGammaTest] FAILED with "
            << g_state.failures
            << " failure(s)."
            << G4endl;

        return 1;
    }

    G4cout
        << "[NativeTwoGammaTest] PASS: "
        << kEventCount
        << " delayed native 2-gamma annihilations with entanglement, "
        << "mean delay "
        << mean_delay_ns
        << " ns (tau_ops "
        << kOrthoLifetimeNs
        << " ns)."
        << G4endl;

    return 0;
}
