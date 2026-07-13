#ifndef PS_SOURCE_ANNIHILATION_PHYSICS_HH
#define PS_SOURCE_ANNIHILATION_PHYSICS_HH

#include "PsSourceAnnihilationConfig.hh"

#include "G4VPhysicsConstructor.hh"

class PsSourceAnnihilationPhysics
    : public G4VPhysicsConstructor {
public:
    explicit PsSourceAnnihilationPhysics(
        const PsSourceAnnihilationConfig& config
    );

    ~PsSourceAnnihilationPhysics() override = default;

    void ConstructParticle() override;
    void ConstructProcess() override;

private:
    PsSourceAnnihilationConfig m_config;
};

#endif
