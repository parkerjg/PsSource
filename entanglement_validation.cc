// Entanglement-modulation validation for PsSource 2-gamma delegation.
//
// A near-rest positron annihilates inside a small water cube at the origin.
// The annihilation photons travel through vacuum into a thick plastic
// spherical shell that acts as an ideal 4-pi Compton polarimeter. For each
// event the first interaction of each annihilation photon is recorded, and
// events in which both photons first Compton scatter in the shell are
// written out with the two polar scattering angles and the azimuthal angle
// between the two scattering planes (delta phi).
//
// Configurations (--mode):
//   native   : no PsSource; native Geant4 at-rest annihilation
//   direct   : PsSource, 100% direct annihilation, Geant4Native 2-gamma
//   pickoff  : PsSource, 100% o-Ps pick-off with exponential delay,
//              Geant4Native 2-gamma
//   legacy   : PsSource, 100% direct annihilation, IdealizedBackToBack
//              2-gamma (PsSource 3.0.0 behavior; negative control)
//
// Entanglement is switched with --qe on|off. Polarized Compton scattering
// uses G4EmLivermorePolarizedPhysics in every configuration.
//
// Output: a CSV of accepted photon pairs, analyzed by
// validate_entanglement_modulation.py.

#include "PsSourceAnnihilationObserver.hh"
#include "PsSourceAnnihilationPhysics.hh"

#include "G4Box.hh"
#include "G4EmLivermorePolarizedPhysics.hh"
#include "G4EmParameters.hh"
#include "G4Event.hh"
#include "G4Gamma.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4ParticleGun.hh"
#include "G4PhysListFactory.hh"
#include "G4Positron.hh"
#include "G4RunManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4Sphere.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Track.hh"
#include "G4UImanager.hh"
#include "G4UserEventAction.hh"
#include "G4UserSteppingAction.hh"
#include "G4VModularPhysicsList.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VProcess.hh"
#include "G4VUserActionInitialization.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4ios.hh"
#include "Randomize.hh"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr double kPickoffLifetimeNs = 2.0;

// Photons whose first interaction happens below this energy have already
// lost energy and are not annihilation photons at their first vertex.
constexpr double kMinimumIncidentEnergyMeV = 0.5;

struct Options {
    std::string mode = "native";
    bool quantum_entanglement = true;
    long events = 100000;
    long seed = 1;
    std::string output = "entanglement_pairs.csv";
    double record_theta_min_deg = 40.0;
    double record_theta_max_deg = 120.0;
};

struct FirstInteraction {
    std::string process;
    bool in_shell = false;
    double incident_energy_MeV = 0.0;
    G4ThreeVector incident_direction;
    G4ThreeVector outgoing_direction;
};

struct EventState {
    std::set<G4int> finished_tracks;
    std::vector<FirstInteraction> interactions;

    int ps_class = -1;
    double delay_ns = 0.0;
};

struct RunCounters {
    long events = 0;
    long two_photon_events = 0;
    long compton_pairs = 0;
    long written_pairs = 0;
};

EventState g_event;
RunCounters g_counters;
std::ofstream g_output;
Options g_options;

void Usage(const char* program)
{
    std::fprintf(
        stderr,
        "Usage: %s --mode native|direct|pickoff|legacy --qe on|off\n"
        "          [--events N] [--seed S] [--out FILE.csv]\n"
        "          [--record-theta-min DEG] [--record-theta-max DEG]\n",
        program
    );
}

bool ParseOptions(
    int argc,
    char** argv,
    Options& options
)
{
    for (int index = 1; index < argc; ++index) {
        const std::string key = argv[index];

        if (index + 1 >= argc) {
            std::fprintf(stderr, "Missing value for %s\n", key.c_str());
            return false;
        }

        const std::string value = argv[++index];

        if (key == "--mode") {
            options.mode = value;
        }
        else if (key == "--qe") {
            if (value != "on" && value != "off") {
                std::fprintf(stderr, "--qe must be on or off\n");
                return false;
            }
            options.quantum_entanglement = (value == "on");
        }
        else if (key == "--events") {
            options.events = std::atol(value.c_str());
        }
        else if (key == "--seed") {
            options.seed = std::atol(value.c_str());
        }
        else if (key == "--out") {
            options.output = value;
        }
        else if (key == "--record-theta-min") {
            options.record_theta_min_deg = std::atof(value.c_str());
        }
        else if (key == "--record-theta-max") {
            options.record_theta_max_deg = std::atof(value.c_str());
        }
        else {
            std::fprintf(stderr, "Unknown option %s\n", key.c_str());
            return false;
        }
    }

    if (
        options.mode != "native" &&
        options.mode != "direct" &&
        options.mode != "pickoff" &&
        options.mode != "legacy"
    ) {
        std::fprintf(stderr, "Unknown mode %s\n", options.mode.c_str());
        return false;
    }

    if (options.events <= 0 || options.seed <= 0) {
        std::fprintf(stderr, "--events and --seed must be positive\n");
        return false;
    }

    return true;
}

}  // namespace

class ClassObserver
    : public IPsSourceAnnihilationObserver {
public:
    void OnPsSourceAnnihilation(
        const PsSourceAnnihilationRecord& record
    ) override
    {
        g_event.ps_class =
            static_cast<int>(record.ps_class);

        g_event.delay_ns =
            record.sampled_ps_delay_ns;
    }
};

class ShellPolarimeter
    : public G4VUserDetectorConstruction {
public:
    G4VPhysicalVolume* Construct() override
    {
        auto* nist = G4NistManager::Instance();

        auto* vacuum =
            nist->FindOrBuildMaterial("G4_Galactic");

        auto* water =
            nist->FindOrBuildMaterial("G4_WATER");

        auto* plastic =
            nist->FindOrBuildMaterial(
                "G4_PLASTIC_SC_VINYLTOLUENE"
            );

        auto* world_solid =
            new G4Box("World", 50.0 * cm, 50.0 * cm, 50.0 * cm);

        auto* world_logical =
            new G4LogicalVolume(world_solid, vacuum, "World");

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

        // Small stopping medium for the positron.
        auto* source_solid =
            new G4Box("Source", 1.0 * mm, 1.0 * mm, 1.0 * mm);

        auto* source_logical =
            new G4LogicalVolume(source_solid, water, "Source");

        new G4PVPlacement(
            nullptr,
            G4ThreeVector(),
            source_logical,
            "Source",
            world_logical,
            false,
            0,
            true
        );

        // Thick plastic shell: every annihilation photon crosses it.
        auto* shell_solid =
            new G4Sphere(
                "Shell",
                5.0 * cm,
                15.0 * cm,
                0.0,
                360.0 * deg,
                0.0,
                180.0 * deg
            );

        auto* shell_logical =
            new G4LogicalVolume(shell_solid, plastic, "Shell");

        new G4PVPlacement(
            nullptr,
            G4ThreeVector(),
            shell_logical,
            "Shell",
            world_logical,
            false,
            0,
            true
        );

        return world_physical;
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

        m_gun.SetParticlePosition(G4ThreeVector());

        m_gun.SetParticleMomentumDirection(
            G4ThreeVector(0.0, 0.0, 1.0)
        );

        m_gun.SetParticleEnergy(0.0001 * keV);
    }

    void GeneratePrimaries(G4Event* event) override
    {
        m_gun.GeneratePrimaryVertex(event);
    }

private:
    G4ParticleGun m_gun;
};

class FirstInteractionRecorder
    : public G4UserSteppingAction {
public:
    void UserSteppingAction(const G4Step* step) override
    {
        const G4Track* track = step->GetTrack();

        if (track->GetDefinition() != G4Gamma::GammaDefinition()) {
            return;
        }

        // Annihilation photons of the primary positron only.
        const G4VProcess* creator = track->GetCreatorProcess();

        if (
            track->GetParentID() != 1 ||
            !creator ||
            creator->GetProcessName() != "annihil"
        ) {
            return;
        }

        if (g_event.finished_tracks.count(track->GetTrackID())) {
            return;
        }

        const G4StepPoint* pre = step->GetPreStepPoint();
        const G4StepPoint* post = step->GetPostStepPoint();

        const G4VProcess* process = post->GetProcessDefinedStep();

        if (!process) {
            return;
        }

        const G4String& name = process->GetProcessName();

        if (name == "Transportation" || name == "CoupledTransportation") {
            return;
        }

        g_event.finished_tracks.insert(track->GetTrackID());

        FirstInteraction interaction;

        interaction.process = name;

        interaction.in_shell =
            pre->GetPhysicalVolume() &&
            pre->GetPhysicalVolume()->GetName() == "Shell";

        interaction.incident_energy_MeV =
            pre->GetKineticEnergy() / MeV;

        interaction.incident_direction =
            pre->GetMomentumDirection();

        interaction.outgoing_direction =
            post->GetMomentumDirection();

        g_event.interactions.push_back(interaction);
    }
};

class PairWriter
    : public G4UserEventAction {
public:
    void BeginOfEventAction(const G4Event*) override
    {
        g_event = EventState();
    }

    void EndOfEventAction(const G4Event*) override
    {
        ++g_counters.events;

        if (g_event.interactions.size() != 2) {
            return;
        }

        ++g_counters.two_photon_events;

        for (const FirstInteraction& interaction : g_event.interactions) {
            if (
                interaction.process != "compt" ||
                !interaction.in_shell ||
                interaction.incident_energy_MeV < kMinimumIncidentEnergyMeV
            ) {
                return;
            }
        }

        ++g_counters.compton_pairs;

        double theta_deg[2];
        G4ThreeVector projection[2];

        for (int index = 0; index < 2; ++index) {
            const FirstInteraction& interaction =
                g_event.interactions[index];

            const G4ThreeVector& k_in =
                interaction.incident_direction;

            const G4ThreeVector& k_out =
                interaction.outgoing_direction;

            const double cos_theta =
                std::max(-1.0, std::min(1.0, k_in.dot(k_out)));

            theta_deg[index] = std::acos(cos_theta) / deg;

            // Scattered direction projected onto the plane perpendicular to
            // the incident direction. For back-to-back photons both
            // projections lie in the same plane, and the angle between them
            // is the angle between the two scattering planes.
            projection[index] = k_out - cos_theta * k_in;
        }

        for (double theta : theta_deg) {
            if (
                theta < g_options.record_theta_min_deg ||
                theta > g_options.record_theta_max_deg
            ) {
                return;
            }
        }

        if (projection[0].mag() <= 0.0 || projection[1].mag() <= 0.0) {
            return;
        }

        const double cos_dphi =
            std::max(
                -1.0,
                std::min(
                    1.0,
                    projection[0].unit().dot(projection[1].unit())
                )
            );

        const double dphi_deg = std::acos(cos_dphi) / deg;

        g_output
            << std::fixed << std::setprecision(4)
            << theta_deg[0] << ','
            << theta_deg[1] << ','
            << dphi_deg << ','
            << g_event.ps_class << ','
            << std::setprecision(6)
            << g_event.delay_ns << '\n';

        ++g_counters.written_pairs;
    }
};

class ValidationActionInitialization
    : public G4VUserActionInitialization {
public:
    void Build() const override
    {
        SetUserAction(new RestPositronGenerator());
        SetUserAction(new FirstInteractionRecorder());
        SetUserAction(new PairWriter());
    }
};

int main(int argc, char** argv)
{
    if (!ParseOptions(argc, argv, g_options)) {
        Usage(argv[0]);
        return 2;
    }

    g_output.open(g_options.output);

    if (!g_output) {
        std::fprintf(
            stderr,
            "Cannot open output file %s\n",
            g_options.output.c_str()
        );
        return 1;
    }

    g_output << "theta1_deg,theta2_deg,dphi_deg,ps_class,delay_ns\n";

    G4Random::setTheSeed(g_options.seed);

    auto* run_manager =
        G4RunManagerFactory::CreateRunManager(
            G4RunManagerType::Serial
        );

    run_manager->SetUserInitialization(new ShellPolarimeter());

    G4PhysListFactory factory;

    G4VModularPhysicsList* physics =
        factory.GetReferencePhysList("FTFP_BERT");

    if (!physics) {
        std::fprintf(stderr, "Failed to create FTFP_BERT.\n");
        delete run_manager;
        return 1;
    }

    physics->SetVerboseLevel(0);

    physics->ReplacePhysics(new G4EmLivermorePolarizedPhysics());

    G4EmParameters::Instance()->SetQuantumEntanglement(
        g_options.quantum_entanglement
    );

    ClassObserver observer;

    if (g_options.mode != "native") {
        PsSourceAnnihilationConfig config;

        config.environment.tau_direct_ns = 0.0;
        config.environment.tau_pps_ns = 0.125;
        config.environment.tau_ops_ns = kPickoffLifetimeNs;
        config.environment.ops_2g_fraction = 1.0;
        config.environment.ops_3g_fraction = 0.0;

        if (g_options.mode == "pickoff") {
            config.environment.f_direct = 0.0;
            config.environment.f_pps = 0.0;
            config.environment.f_ops = 1.0;
        }
        else {
            config.environment.f_direct = 1.0;
            config.environment.f_pps = 0.0;
            config.environment.f_ops = 0.0;
        }

        config.delay_mode = PsSourceDelayMode::Exponential;
        config.enable_three_gamma = false;

        config.two_gamma_model =
            (g_options.mode == "legacy")
                ? PsSourceTwoGammaModel::IdealizedBackToBack
                : PsSourceTwoGammaModel::Geant4Native;

        config.observer = &observer;

        physics->RegisterPhysics(
            new PsSourceAnnihilationPhysics(config)
        );
    }

    run_manager->SetUserInitialization(physics);

    run_manager->SetUserInitialization(
        new ValidationActionInitialization()
    );

    G4UImanager* ui = G4UImanager::GetUIpointer();
    ui->ApplyCommand("/control/verbose 0");
    ui->ApplyCommand("/run/verbose 0");
    ui->ApplyCommand("/event/verbose 0");
    ui->ApplyCommand("/tracking/verbose 0");
    ui->ApplyCommand("/process/em/verbose 0");
    ui->ApplyCommand("/process/had/verbose 0");

    run_manager->Initialize();
    run_manager->BeamOn(static_cast<G4int>(g_options.events));

    g_output.close();

    std::printf(
        "[EntanglementValidation] mode=%s qe=%s seed=%ld events=%ld "
        "two_photon_events=%ld compton_pairs=%ld written_pairs=%ld\n",
        g_options.mode.c_str(),
        g_options.quantum_entanglement ? "on" : "off",
        g_options.seed,
        g_counters.events,
        g_counters.two_photon_events,
        g_counters.compton_pairs,
        g_counters.written_pairs
    );

    delete run_manager;
    return 0;
}
