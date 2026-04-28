#ifndef POSITRONIUM_TRUTH_INFO_HH
#define POSITRONIUM_TRUTH_INFO_HH

#include "G4VUserEventInformation.hh"

#include <cstdint>
#include <limits>

class PositroniumTruthInfo : public G4VUserEventInformation {
public:
    enum class GenerationMode : int {
        Unknown        = -1,
        NativeGeant4   = 0,
        ExplicitProvider = 1
    };

    enum class SourceTag : int {
        Unknown                 = -1,
        NativePositronSource    = 0,
        ExplicitPhotonVertices  = 1
    };

    enum class DelayMode : int {
        Unknown      = -1,
        Exponential  = 0,
        Fixed        = 1
    };

    PositroniumTruthInfo() = default;
    virtual ~PositroniumTruthInfo() = default;

    virtual void Print() const override {}

    // -------------------------------------------------------------------------
    // Event identity / provenance
    // -------------------------------------------------------------------------
    uint64_t source_event_id = 0;

    GenerationMode generation_mode = GenerationMode::Unknown;
    SourceTag source_tag = SourceTag::Unknown;

    // Convenience mirror for quick checks in downstream code.
    bool source_is_explicit = false;

    // -------------------------------------------------------------------------
    // Requested source-model configuration
    // -------------------------------------------------------------------------
    // These record what the generator/provider was asked to do.
    double requested_f_direct = -1.0;
    double requested_f_pps = -1.0;
    double requested_f_ops = -1.0;

    double requested_ortho_3g_fraction = -1.0;

    DelayMode requested_delay_mode = DelayMode::Unknown;
    double requested_tau_pps_ns = -1.0;
    double requested_tau_ops_ns = -1.0;
    double requested_fixed_delay_ns = -1.0;

    bool requested_enable_positron_range = false;
    double requested_positron_range_sigma_mm = -1.0;

    // Native-Geant4-side material branch steering, when applicable.
    bool requested_native_orto_ps_fraction_valid = false;
    double requested_native_orto_ps_fraction = -1.0;

    // -------------------------------------------------------------------------
    // Event-level realized truth
    // -------------------------------------------------------------------------
    // ps_class_id should follow the PsClass enum used by TimedEventModel.
    int ps_class_id = -1;

    // 2 for two-gamma annihilation, 3 for three-gamma annihilation, -1 unknown.
    int annihilation_mode = -1;

    // Declared annihilation truth is fully known in explicit mode at generation
    // time, but may be unknown at source generation in native mode and filled
    // later from tracking/event actions.
    bool declared_annihilation_valid = false;

    // Delay from source creation to annihilation vertex.
    double delay_ns = -1.0;

    bool has_prompt_gamma = false;
    double prompt_energy_MeV = 0.0;

    // Source position
    double source_x_mm = 0.0;
    double source_y_mm = 0.0;
    double source_z_mm = 0.0;

    // Declared annihilation position
    double ann_x_mm = std::numeric_limits<double>::quiet_NaN();
    double ann_y_mm = std::numeric_limits<double>::quiet_NaN();
    double ann_z_mm = std::numeric_limits<double>::quiet_NaN();

    double positron_range_mm = -1.0;
    int medium_id = -1;
    double local_tau_ns = -1.0;

    // QE request / metadata.
    bool qe_mode = true;
};

#endif
