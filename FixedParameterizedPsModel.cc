#include "FixedParameterizedPsModel.hh"

#include "Randomize.hh"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

constexpr double kElectronMassMeV = 0.510999;
constexpr double kTwoGammaEnergyMeV = kElectronMassMeV;
constexpr double kThreeGammaTotalEnergyMeV = 2.0 * kElectronMassMeV;

constexpr double kPi = 3.14159265358979323846;
constexpr double kFractionTolerance = 1.0e-9;
constexpr double kMinimumThreeGammaEnergyMeV = 1.0e-6;

using Vec3 = std::array<double, 3>;

double Dot(const Vec3& a, const Vec3& b)
{
    return
        a[0] * b[0] +
        a[1] * b[1] +
        a[2] * b[2];
}

Vec3 Cross(const Vec3& a, const Vec3& b)
{
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]
    };
}

double Norm(const Vec3& v)
{
    return std::sqrt(std::max(0.0, Dot(v, v)));
}

Vec3 Normalize(const Vec3& v)
{
    const double norm = Norm(v);

    if (norm <= 0.0) {
        throw std::runtime_error(
            "Cannot normalize a zero-length vector."
        );
    }

    return {
        v[0] / norm,
        v[1] / norm,
        v[2] / norm
    };
}

Vec3 Scale(const Vec3& v, double scale)
{
    return {
        scale * v[0],
        scale * v[1],
        scale * v[2]
    };
}

Vec3 Add(const Vec3& a, const Vec3& b)
{
    return {
        a[0] + b[0],
        a[1] + b[1],
        a[2] + b[2]
    };
}

Vec3 Negate(const Vec3& v)
{
    return {
        -v[0],
        -v[1],
        -v[2]
    };
}

// Select the Cartesian axis least aligned with the supplied direction.
// This avoids numerical degeneracy without imposing a fixed preferred axis.
Vec3 LeastAlignedCartesianAxis(const Vec3& direction)
{
    const double ax = std::abs(direction[0]);
    const double ay = std::abs(direction[1]);
    const double az = std::abs(direction[2]);

    if (ax <= ay && ax <= az) {
        return {1.0, 0.0, 0.0};
    }

    if (ay <= ax && ay <= az) {
        return {0.0, 1.0, 0.0};
    }

    return {0.0, 0.0, 1.0};
}

}  // namespace

FixedParameterizedPsModel::FixedParameterizedPsModel() = default;

void FixedParameterizedPsModel::SetDelayMode(DelayMode mode)
{
    m_delay_mode = mode;
}

void FixedParameterizedPsModel::SetFixedDelayNs(double fixed_delay_ns)
{
    if (fixed_delay_ns < 0.0) {
        throw std::invalid_argument(
            "Fixed positronium delay must be non-negative."
        );
    }

    m_fixed_delay_ns = fixed_delay_ns;
}

void FixedParameterizedPsModel::SetEnableThreeGamma(bool enabled)
{
    m_enable_three_gamma = enabled;
}

PsBranchResult FixedParameterizedPsModel::SampleBranch(
    const PsEnvironment& environment
) const
{
    ValidateEnvironment(environment);

    PsBranchResult result;

    result.ps_class =
        SamplePsClass(environment);

    result.delay_ns =
        SampleDelayNs(
            result.ps_class,
            environment
        );

    result.annihilation_mode =
        (result.ps_class == PsClass::OrthoPs3g)
            ? 3
            : 2;

    return result;
}

std::vector<PsPhoton>
FixedParameterizedPsModel::SamplePhotons(
    PsClass ps_class
) const
{
    std::vector<PsPhoton> photons;

    const int annihilation_mode =
        (ps_class == PsClass::OrthoPs3g)
            ? 3
            : 2;

    photons.reserve(
        static_cast<std::size_t>(
            annihilation_mode
        )
    );

    if (annihilation_mode == 2) {
        const auto directions =
            SampleBackToBackDirections();

        PsPhoton photon1;
        photon1.kinetic_energy_MeV =
            kTwoGammaEnergyMeV;
        photon1.direction =
            directions.first;

        PsPhoton photon2;
        photon2.kinetic_energy_MeV =
            kTwoGammaEnergyMeV;
        photon2.direction =
            directions.second;

        photons.push_back(photon1);
        photons.push_back(photon2);

        return photons;
    }

    std::array<double, 3> energies_MeV{};
    std::array<Vec3, 3> directions{};

    SampleThreeGammaKinematics(
        energies_MeV,
        directions
    );

    for (
        std::size_t index = 0;
        index < 3;
        ++index
    ) {
        PsPhoton photon;

        photon.kinetic_energy_MeV =
            energies_MeV[index];

        photon.direction =
            directions[index];

        photons.push_back(photon);
    }

    return photons;
}

PsModelResult FixedParameterizedPsModel::Sample(
    const PsEnvironment& environment
) const
{
    const PsBranchResult branch =
        SampleBranch(environment);

    PsModelResult result;
    result.ps_class = branch.ps_class;
    result.annihilation_mode =
        branch.annihilation_mode;
    result.delay_ns = branch.delay_ns;
    result.photons =
        SamplePhotons(branch.ps_class);

    result.model_name = Name();
    result.model_version = Version();
    result.validation_status =
        ValidationStatus();

    return result;
}

std::string FixedParameterizedPsModel::Name() const
{
    return "FixedParameterizedPsModel";
}

std::string FixedParameterizedPsModel::Version() const
{
    return "1.0";
}

std::string FixedParameterizedPsModel::ValidationStatus() const
{
    return "approximate-controlled-source-model";
}

void FixedParameterizedPsModel::ValidateEnvironment(
    const PsEnvironment& environment
) const
{
    if (
        environment.f_direct < 0.0 ||
        environment.f_pps < 0.0 ||
        environment.f_ops < 0.0
    ) {
        throw std::invalid_argument(
            "Positronium branch fractions must be non-negative."
        );
    }

    const double total_fraction =
        environment.f_direct +
        environment.f_pps +
        environment.f_ops;

    if (
        std::abs(total_fraction - 1.0) >
        kFractionTolerance
    ) {
        throw std::invalid_argument(
            "Direct, para-Ps, and ortho-Ps fractions must sum to 1."
        );
    }

    if (environment.tau_direct_ns < 0.0) {
        throw std::invalid_argument(
            "Direct-annihilation lifetime must be non-negative."
        );
    }

    if (environment.tau_pps_ns <= 0.0) {
        throw std::invalid_argument(
            "Para-Ps lifetime must be strictly positive."
        );
    }

    if (environment.tau_ops_ns <= 0.0) {
        throw std::invalid_argument(
            "Ortho-Ps lifetime must be strictly positive."
        );
    }

    if (
        environment.ops_2g_fraction < 0.0 ||
        environment.ops_2g_fraction > 1.0
    ) {
        throw std::invalid_argument(
            "Ortho-Ps 2-gamma fraction must be in [0,1]."
        );
    }

    if (
        environment.ops_3g_fraction < 0.0 ||
        environment.ops_3g_fraction > 1.0
    ) {
        throw std::invalid_argument(
            "Ortho-Ps 3-gamma fraction must be in [0,1]."
        );
    }

    const double ortho_total =
        environment.ops_2g_fraction +
        environment.ops_3g_fraction;

    if (
        std::abs(ortho_total - 1.0) >
        kFractionTolerance
    ) {
        throw std::invalid_argument(
            "Ortho-Ps 2-gamma and 3-gamma fractions must sum to 1."
        );
    }
}

PsClass FixedParameterizedPsModel::SamplePsClass(
    const PsEnvironment& environment
) const
{
    const double branch_sample = G4UniformRand();

    if (branch_sample < environment.f_direct) {
        return PsClass::Direct2g;
    }

    if (
        branch_sample <
        environment.f_direct + environment.f_pps
    ) {
        return PsClass::ParaPs2g;
    }

    if (!m_enable_three_gamma) {
        return PsClass::OrthoPs2g;
    }

    const double ortho_sample = G4UniformRand();

    if (
        ortho_sample <
        environment.ops_3g_fraction
    ) {
        return PsClass::OrthoPs3g;
    }

    return PsClass::OrthoPs2g;
}

double FixedParameterizedPsModel::SampleDelayNs(
    PsClass ps_class,
    const PsEnvironment& environment
) const
{
    if (m_delay_mode == DelayMode::Fixed) {
        if (ps_class == PsClass::Direct2g) {
            return environment.tau_direct_ns;
        }

        return m_fixed_delay_ns;
    }

    double tau_ns = 0.0;

    switch (ps_class) {
        case PsClass::Direct2g:
            tau_ns = environment.tau_direct_ns;
            break;

        case PsClass::ParaPs2g:
            tau_ns = environment.tau_pps_ns;
            break;

        case PsClass::OrthoPs2g:
        case PsClass::OrthoPs3g:
            tau_ns = environment.tau_ops_ns;
            break;
    }

    if (tau_ns == 0.0) {
        return 0.0;
    }

    return -tau_ns * std::log(
        SampleUniformOpen()
    );
}

std::array<double, 3>
FixedParameterizedPsModel::SampleIsotropicDirection() const
{
    const double cos_theta =
        -1.0 + 2.0 * G4UniformRand();

    const double sin_theta = std::sqrt(
        std::max(
            0.0,
            1.0 - cos_theta * cos_theta
        )
    );

    const double phi =
        2.0 * kPi * G4UniformRand();

    return {
        sin_theta * std::cos(phi),
        sin_theta * std::sin(phi),
        cos_theta
    };
}

std::pair<
    std::array<double, 3>,
    std::array<double, 3>
>
FixedParameterizedPsModel::SampleBackToBackDirections() const
{
    const Vec3 direction1 =
        SampleIsotropicDirection();

    const Vec3 direction2 =
        Negate(direction1);

    return {
        direction1,
        direction2
    };
}

void FixedParameterizedPsModel::SampleThreeGammaKinematics(
    std::array<double, 3>& energies_MeV,
    std::array<std::array<double, 3>, 3>& directions
) const
{
    // This is an approximate constrained phase-space sampler.
    //
    // It guarantees:
    //   - positive photon energies;
    //   - total energy equal to 2*m_e*c^2;
    //   - exact momentum closure;
    //   - isotropic event-plane orientation.
    //
    // It does not reproduce the QED-weighted Ore-Powell distribution.

    while (true) {
        const double sample1 =
            -std::log(SampleUniformOpen());

        const double sample2 =
            -std::log(SampleUniformOpen());

        const double sample3 =
            -std::log(SampleUniformOpen());

        const double sample_sum =
            sample1 + sample2 + sample3;

        if (sample_sum <= 0.0) {
            continue;
        }

        energies_MeV[0] =
            kThreeGammaTotalEnergyMeV *
            sample1 / sample_sum;

        energies_MeV[1] =
            kThreeGammaTotalEnergyMeV *
            sample2 / sample_sum;

        energies_MeV[2] =
            kThreeGammaTotalEnergyMeV *
            sample3 / sample_sum;

        const double maximum_energy =
            std::max(
                energies_MeV[0],
                std::max(
                    energies_MeV[1],
                    energies_MeV[2]
                )
            );

        if (
            energies_MeV[0] <= kMinimumThreeGammaEnergyMeV ||
            energies_MeV[1] <= kMinimumThreeGammaEnergyMeV ||
            energies_MeV[2] <= kMinimumThreeGammaEnergyMeV
        ) {
            continue;
        }

        if (
            maximum_energy <=
            0.5 * kThreeGammaTotalEnergyMeV
        ) {
            break;
        }
    }

    const Vec3 plane_normal =
        SampleIsotropicDirection();

    const Vec3 reference_axis =
        LeastAlignedCartesianAxis(plane_normal);

    const Vec3 basis_u =
        Normalize(
            Cross(
                reference_axis,
                plane_normal
            )
        );

    const Vec3 basis_v =
        Normalize(
            Cross(
                plane_normal,
                basis_u
            )
        );

    // Randomly rotate the coordinate basis within the sampled event plane.
    // This prevents the first photon direction from inheriting structure from
    // the Cartesian reference axis used only to construct the basis.
    const double in_plane_angle =
        2.0 * kPi * G4UniformRand();

    const double cos_in_plane =
        std::cos(in_plane_angle);

    const double sin_in_plane =
        std::sin(in_plane_angle);

    const Vec3 rotated_u = Add(
        Scale(basis_u, cos_in_plane),
        Scale(basis_v, sin_in_plane)
    );

    const Vec3 rotated_v = Add(
        Scale(basis_u, -sin_in_plane),
        Scale(basis_v, cos_in_plane)
    );

    const double energy1 = energies_MeV[0];
    const double energy2 = energies_MeV[1];
    const double energy3 = energies_MeV[2];

    double cos_angle12 =
        (
            energy3 * energy3 -
            energy1 * energy1 -
            energy2 * energy2
        ) /
        (
            2.0 *
            energy1 *
            energy2
        );

    cos_angle12 = std::clamp(
        cos_angle12,
        -1.0,
        1.0
    );

    double sin_angle12 = std::sqrt(
        std::max(
            0.0,
            1.0 -
            cos_angle12 * cos_angle12
        )
    );

    if (G4UniformRand() < 0.5) {
        sin_angle12 = -sin_angle12;
    }

    const Vec3 direction1 = rotated_u;

    const Vec3 direction2 = Add(
        Scale(
            rotated_u,
            cos_angle12
        ),
        Scale(
            rotated_v,
            sin_angle12
        )
    );

    const Vec3 momentum1 =
        Scale(
            direction1,
            energy1
        );

    const Vec3 momentum2 =
        Scale(
            direction2,
            energy2
        );

    const Vec3 momentum3 =
        Negate(
            Add(
                momentum1,
                momentum2
            )
        );

    const Vec3 direction3 =
        Normalize(momentum3);

    directions[0] =
        Normalize(direction1);

    directions[1] =
        Normalize(direction2);

    directions[2] =
        direction3;
}

double FixedParameterizedPsModel::SampleUniformOpen() const
{
    double sample = G4UniformRand();

    while (
        sample <= 0.0 ||
        sample >= 1.0
    ) {
        sample = G4UniformRand();
    }

    return sample;
}
