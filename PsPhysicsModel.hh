#ifndef PS_PHYSICS_MODEL_HH
#define PS_PHYSICS_MODEL_HH

#include "TimedEventModel.hh"

#include <array>
#include <string>
#include <vector>

// Local properties available to the positronium physics model.
//
// Initially these values will come from fixed configuration parameters.
// Later they can be supplied by a Geant4 material, region, phantom,
// or user-defined environment provider.
struct PsEnvironment {
    int medium_id = 0;

    double f_direct = 0.3;
    double f_pps = 0.2;
    double f_ops = 0.5;

    double tau_direct_ns = 0.0;
    double tau_pps_ns = 0.125;
    double tau_ops_ns = 3.0;

    double ops_2g_fraction = 0.0;
    double ops_3g_fraction = 1.0;
};

// One photon returned by a positronium physics model.
struct PsPhoton {
    double kinetic_energy_MeV = 0.0;

    std::array<double, 3> direction = {
        1.0, 0.0, 0.0
    };

    // Optional polarization vector.
    std::array<double, 3> polarization = {
        0.0, 0.0, 0.0
    };

    bool polarization_valid = false;
};

struct PsBranchResult {
    PsClass ps_class = PsClass::Direct2g;
    int annihilation_mode = 2;
    double delay_ns = 0.0;
};

// Result returned by a positronium physics model for one annihilation.
struct PsModelResult {
    PsClass ps_class = PsClass::Direct2g;

    int annihilation_mode = 2;
    double delay_ns = 0.0;

    std::vector<PsPhoton> photons;

    // Model provenance recorded in downstream truth/configuration outputs.
    std::string model_name;
    std::string model_version;
    std::string validation_status;
};

// Interface implemented by interchangeable positronium physics models.
class IPsPhysicsModel {
public:
    virtual ~IPsPhysicsModel() = default;

    virtual PsModelResult Sample(
        const PsEnvironment& environment
    ) const = 0;

    virtual std::string Name() const = 0;
    virtual std::string Version() const = 0;
    virtual std::string ValidationStatus() const = 0;
};

#endif
