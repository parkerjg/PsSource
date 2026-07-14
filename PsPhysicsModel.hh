#ifndef PS_PHYSICS_MODEL_HH
#define PS_PHYSICS_MODEL_HH

#include "PsSourceTypes.hh"
#include "TimedEventModel.hh"

#include <array>
#include <string>
#include <vector>

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
