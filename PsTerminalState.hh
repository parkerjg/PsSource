#ifndef PS_TERMINAL_STATE_HH
#define PS_TERMINAL_STATE_HH

#include "G4ThreeVector.hh"

#include <cstdint>
#include <vector>

class G4LogicalVolume;
class G4Material;
class G4Region;
class G4VPhysicalVolume;

// Geant4 transport state captured when a positron reaches the
// terminal-state handoff used by transport-coupled PsSource.
//
// This record describes the computational handoff point. It does not
// imply that microscopic positronium formation occurs only at rest.
struct PsTerminalState {
    // Event and track identity
    std::uint64_t source_event_id = 0;

    int positron_track_id = -1;
    int parent_track_id = -1;

    // Source state
    G4ThreeVector source_position;
    double source_time_ns = 0.0;

    // Terminal transported-positron state
    G4ThreeVector terminal_position;
    double terminal_time_ns = 0.0;

    double initial_kinetic_energy_MeV = 0.0;
    double terminal_kinetic_energy_MeV = 0.0;

    // Transport metrics
    double track_length_mm = 0.0;
    double source_to_terminal_distance_mm = 0.0;

    // Local Geant4 environment at the handoff point
    const G4Material* material = nullptr;
    const G4Region* region = nullptr;
    const G4LogicalVolume* logical_volume = nullptr;
    const G4VPhysicalVolume* physical_volume = nullptr;

    int copy_number = -1;

    // Touchable hierarchy ordered from the current volume outward.
    std::vector<int> touchable_copy_numbers;
};

#endif
