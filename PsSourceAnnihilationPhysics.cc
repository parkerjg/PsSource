#include "PsSourceAnnihilationPhysics.hh"

#include "PsSourceAnnihilationProcess.hh"

#include "G4Exception.hh"
#include "G4ExceptionSeverity.hh"
#include "G4ParticleDefinition.hh"
#include "G4Positron.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessVector.hh"
#include "G4VProcess.hh"
#include "G4ios.hh"

PsSourceAnnihilationPhysics::
PsSourceAnnihilationPhysics()
    : G4VPhysicsConstructor(
          "PsSourceAnnihilationPhysics"
      )
{
}

void PsSourceAnnihilationPhysics::
ConstructParticle()
{
    G4Positron::PositronDefinition();
}

void PsSourceAnnihilationPhysics::
ConstructProcess()
{
    G4ParticleDefinition* positron =
        G4Positron::PositronDefinition();

    if (!positron) {
        G4Exception(
            "PsSourceAnnihilationPhysics::ConstructProcess",
            "PsSource_AnnihilationPhysics_001",
            FatalException,
            "Failed to obtain the Geant4 positron definition."
        );
        return;
    }

    G4ProcessManager* process_manager =
        positron->GetProcessManager();

    if (!process_manager) {
        G4Exception(
            "PsSourceAnnihilationPhysics::ConstructProcess",
            "PsSource_AnnihilationPhysics_002",
            FatalException,
            "The positron has no process manager."
        );
        return;
    }

    G4ProcessVector* process_list =
        process_manager->GetProcessList();

    const G4int process_count =
        process_manager->GetProcessListLength();

    G4VProcess* existing_annihilation = nullptr;
    G4int annihilation_matches = 0;

    for (
        G4int index = 0;
        index < process_count;
        ++index
    ) {
        G4VProcess* process =
            (*process_list)[index];

        if (
            process &&
            process->GetProcessName() == "annihil"
        ) {
            existing_annihilation = process;
            ++annihilation_matches;
        }
    }

    if (annihilation_matches != 1) {
        G4ExceptionDescription message;
        message
            << "Expected exactly one positron process named "
            << "'annihil', but found "
            << annihilation_matches
            << ".";

        G4Exception(
            "PsSourceAnnihilationPhysics::ConstructProcess",
            "PsSource_AnnihilationPhysics_003",
            FatalException,
            message
        );
        return;
    }

    G4VProcess* removed_process =
        process_manager->RemoveProcess(
            existing_annihilation
        );

    if (removed_process != existing_annihilation) {
        G4Exception(
            "PsSourceAnnihilationPhysics::ConstructProcess",
            "PsSource_AnnihilationPhysics_004",
            FatalException,
            "Failed to remove the existing annihilation process."
        );
        return;
    }

    delete removed_process;

    auto* replacement =
        new PsSourceAnnihilationProcess("annihil");

    const G4bool registered =
        RegisterProcess(
            replacement,
            positron
        );

    if (!registered) {
        delete replacement;

        G4Exception(
            "PsSourceAnnihilationPhysics::ConstructProcess",
            "PsSource_AnnihilationPhysics_005",
            FatalException,
            "Failed to register the PsSource annihilation process."
        );
        return;
    }

    G4cout
        << "[PsSource] Replaced positron process "
        << "'annihil' with PsSourceAnnihilationProcess."
        << G4endl;
}
