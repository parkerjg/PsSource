#ifndef PS_SOURCE_ANNIHILATION_OBSERVER_HH
#define PS_SOURCE_ANNIHILATION_OBSERVER_HH

#include "PsSourceTypes.hh"

#include <array>
#include <string>

struct PsSourceAnnihilationRecord {
    // Geant4 event and positron track identifiers, for joining this record
    // with other hits in the same event (for example, a prompt gamma).
    // event_id is -1 when no Geant4 event is being processed.
    int event_id = -1;
    int positron_track_id = 0;

    PsClass ps_class = PsClass::Direct2g;
    int annihilation_mode = 2;

    double positron_terminal_time_ns = 0.0;
    double sampled_ps_delay_ns = 0.0;
    double annihilation_time_ns = 0.0;

    std::array<double, 3> annihilation_position_mm = {
        0.0, 0.0, 0.0
    };

    int medium_id = 0;
    double local_tau_ns = 0.0;

    std::string model_name;
    std::string model_version;
    std::string validation_status;
};

class IPsSourceAnnihilationObserver {
public:
    virtual ~IPsSourceAnnihilationObserver() = default;

    virtual void OnPsSourceAnnihilation(
        const PsSourceAnnihilationRecord& record
    ) = 0;
};

#endif
