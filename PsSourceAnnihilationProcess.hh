#ifndef PS_SOURCE_ANNIHILATION_PROCESS_HH
#define PS_SOURCE_ANNIHILATION_PROCESS_HH

#include "G4eplusAnnihilation.hh"

class G4Step;
class G4Track;
class G4VParticleChange;

class PsSourceAnnihilationProcess : public G4eplusAnnihilation {
public:
    explicit PsSourceAnnihilationProcess(
        const G4String& name = "annihil"
    );

    ~PsSourceAnnihilationProcess() override = default;

    G4VParticleChange* AtRestDoIt(
        const G4Track& track,
        const G4Step& step
    ) override;
};

#endif
