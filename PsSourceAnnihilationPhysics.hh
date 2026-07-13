#ifndef PS_SOURCE_ANNIHILATION_PHYSICS_HH
#define PS_SOURCE_ANNIHILATION_PHYSICS_HH

#include "G4VPhysicsConstructor.hh"

class PsSourceAnnihilationPhysics
    : public G4VPhysicsConstructor {
public:
    PsSourceAnnihilationPhysics();

    ~PsSourceAnnihilationPhysics() override = default;

    void ConstructParticle() override;
    void ConstructProcess() override;
};

#endif
