#ifndef TIMED_EVENT_MODEL_HH
#define TIMED_EVENT_MODEL_HH

#include <array>
#include <cstdint>
#include <string>
#include <vector>

enum class PsClass : int {
    Direct2g = 0,
    ParaPs2g = 1,
    OrthoPs2g = 2,
    OrthoPs3g = 3
};

enum class PhotonRole : int {
    Prompt = 0,
    Ann1   = 1,
    Ann2   = 2,
    Ann3   = 3
};

struct ParticleSpec {
    int pdg_code;
    double kinetic_energy_MeV;
    std::array<double, 3> direction;

    int photon_role;
    int parent_vertex_id;
    int parent_ps_class;
    double birth_time_ns;
};

struct VertexSpec {
    int vertex_id;
    double time_ns;
    std::array<double, 3> position;
    std::vector<ParticleSpec> particles;
};

struct TimedEventSpec {
    std::vector<VertexSpec> vertices;

    std::string physics_model_name;
    std::string physics_model_version;
    std::string physics_validation_status;

    uint64_t source_event_id;

    int ps_class_id;
    int annihilation_mode;
    double delay_ns;

    bool has_prompt_gamma;
    double prompt_energy_MeV;

    std::array<double, 3> source_position_mm;
    std::array<double, 3> annihilation_position_mm;

    double positron_range_mm;
    int medium_id;
    double local_tau_ns;

    bool qe_mode;
};

#endif
