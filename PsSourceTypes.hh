#ifndef PS_SOURCE_TYPES_HH
#define PS_SOURCE_TYPES_HH

enum class PsClass : int {
    Direct2g = 0,
    ParaPs2g = 1,
    OrthoPs2g = 2,
    OrthoPs3g = 3
};

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

enum class PsSourceDelayMode {
    Fixed,
    Exponential
};

enum class PsSourceThreeGammaModel {
    ApproximatePhaseSpace,
    Geant4OrePowell,
    Geant4PolarizedOrePowell
};

// Photon generation for the 2-gamma classes (direct, p-Ps, o-Ps pick-off).
//
// Geant4Native delegates the photon final state to the native Geant4
// at-rest annihilation (G4eplusAnnihilation), so quantum entanglement,
// polarization, and acolinearity follow the host's G4EmParameters
// configuration. PsSource only shifts the photon emission time by the
// sampled positronium delay.
//
// IdealizedBackToBack reproduces the PsSource 3.0 behavior: exactly
// collinear 511 keV photons without polarization or entanglement.
enum class PsSourceTwoGammaModel {
    Geant4Native,
    IdealizedBackToBack
};

#endif
