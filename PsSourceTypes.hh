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

#endif
