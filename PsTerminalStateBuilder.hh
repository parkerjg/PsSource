#ifndef PS_TERMINAL_STATE_BUILDER_HH
#define PS_TERMINAL_STATE_BUILDER_HH

#include "PsTerminalState.hh"

class G4Step;
class G4Track;

class PsTerminalStateBuilder {
public:
    static PsTerminalState Build(
        const G4Track& track,
        const G4Step& step,
        std::uint64_t source_event_id
    );
};

#endif
