#include "PsSourceAnnihilationProcess.hh"

#include "G4DynamicParticle.hh"
#include "G4Gamma.hh"
#include "G4ParticleChangeForGamma.hh"
#include "G4PhysicalConstants.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Track.hh"
#include "G4VParticleChange.hh"
#include "Randomize.hh"

#include <cmath>

PsSourceAnnihilationProcess::
PsSourceAnnihilationProcess(
    const G4String& name
)
    : G4eplusAnnihilation(name)
{
}

G4VParticleChange*
PsSourceAnnihilationProcess::AtRestDoIt(
    const G4Track& track,
    const G4Step&
)
{
    auto* particle_change =
        GetParticleChange();

    particle_change->Initialize(track);

    particle_change->SetNumberOfSecondaries(2);

    particle_change->SetProposedKineticEnergy(0.0);

    particle_change->ProposeLocalEnergyDeposit(0.0);

    particle_change->ProposeTrackStatus(
        fStopAndKill
    );

    const G4double cos_theta =
        2.0 * G4UniformRand() - 1.0;

    const G4double sin_theta =
        std::sqrt(
            1.0 - cos_theta * cos_theta
        );

    const G4double phi =
        twopi * G4UniformRand();

    const G4ThreeVector direction(
        sin_theta * std::cos(phi),
        sin_theta * std::sin(phi),
        cos_theta
    );

    const G4double photon_energy =
        electron_mass_c2;

    auto* gamma_definition =
        G4Gamma::GammaDefinition();

    auto* gamma_one =
        new G4DynamicParticle(
            gamma_definition,
            direction,
            photon_energy
        );

    auto* gamma_two =
        new G4DynamicParticle(
            gamma_definition,
            -direction,
            photon_energy
        );

    particle_change->AddSecondary(
        gamma_one
    );

    particle_change->AddSecondary(
        gamma_two
    );

    return particle_change;
}
