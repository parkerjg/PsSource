#include "PsSourceAnnihilationProcess.hh"

#include "G4Step.hh"
#include "G4Track.hh"
#include "G4VParticleChange.hh"

PsSourceAnnihilationProcess::PsSourceAnnihilationProcess(
    const G4String& name
)
    : G4eplusAnnihilation(name)
{
}

G4VParticleChange*
PsSourceAnnihilationProcess::AtRestDoIt(
    const G4Track& track,
    const G4Step& step
)
{
    // Initial gating-process spike:
    // preserve native Geant4 at-rest annihilation unchanged.
    return G4eplusAnnihilation::AtRestDoIt(
        track,
        step
    );
}
