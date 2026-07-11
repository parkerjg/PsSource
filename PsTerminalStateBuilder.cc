#include "PsTerminalStateBuilder.hh"

#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4Region.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4TouchableHandle.hh"
#include "G4Track.hh"
#include "G4VPhysicalVolume.hh"
#include "G4SystemOfUnits.hh"

#include <algorithm>

PsTerminalState PsTerminalStateBuilder::Build(
    const G4Track& track,
    const G4Step& step,
    std::uint64_t source_event_id
)
{
    PsTerminalState state;

    state.source_event_id = source_event_id;

    state.positron_track_id = track.GetTrackID();
    state.parent_track_id = track.GetParentID();

    state.source_position = track.GetVertexPosition();
    state.source_time_ns =
        track.GetGlobalTime() / ns -
        track.GetLocalTime() / ns;

    state.terminal_position = track.GetPosition();
    state.terminal_time_ns =
        track.GetGlobalTime() / ns;

    state.initial_kinetic_energy_MeV =
        track.GetVertexKineticEnergy() / MeV;

    state.terminal_kinetic_energy_MeV =
        track.GetKineticEnergy() / MeV;

    state.track_length_mm =
        track.GetTrackLength() / mm;

    state.source_to_terminal_distance_mm =
        (
            state.terminal_position -
            state.source_position
        ).mag() / mm;

    const G4StepPoint* post_step =
        step.GetPostStepPoint();

    if (post_step) {
        state.material = post_step->GetMaterial();

        const G4TouchableHandle& touchable =
            post_step->GetTouchableHandle();

        if (touchable) {
            state.physical_volume =
                touchable->GetVolume();

            if (state.physical_volume) {
                state.logical_volume =
                    state.physical_volume->
                    GetLogicalVolume();

                state.copy_number =
                    touchable->GetCopyNumber();

                if (state.logical_volume) {
                    state.region =
                        state.logical_volume->
                        GetRegion();
                }
            }

            const int history_depth =
                touchable->GetHistoryDepth();

            state.touchable_copy_numbers.reserve(
                static_cast<std::size_t>(
                    history_depth + 1
                )
            );

            for (
                int depth = 0;
                depth <= history_depth;
                ++depth
            ) {
                state.touchable_copy_numbers.push_back(
                    touchable->GetCopyNumber(depth)
                );
            }
        }
    }

    return state;
}
