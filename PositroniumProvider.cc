#include "PositroniumProvider.hh"
#include "Randomize.hh"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kFractionTolerance = 1.0e-9;

}  // namespace

PositroniumProvider::PositroniumProvider()
{
    m_environment.medium_id = 0;

    m_environment.f_direct = 0.3;
    m_environment.f_pps = 0.2;
    m_environment.f_ops = 0.5;

    m_environment.tau_direct_ns = 0.0;
    m_environment.tau_pps_ns = 0.125;
    m_environment.tau_ops_ns = 3.0;

    m_environment.ops_2g_fraction = 0.0;
    m_environment.ops_3g_fraction = 1.0;

    ValidateFractionTriplet(
        m_environment.f_direct,
        m_environment.f_pps,
        m_environment.f_ops
    );

    m_physics_model.SetEnableThreeGamma(true);
    m_physics_model.SetDelayMode(
        ConfigurablePsModel::DelayMode::Exponential
    );
    m_physics_model.SetFixedDelayNs(3.0);
}

void PositroniumProvider::SetFractions(
    double f_direct,
    double f_pps,
    double f_ops
)
{
    ValidateFractionTriplet(
        f_direct,
        f_pps,
        f_ops
    );

    m_environment.f_direct = f_direct;
    m_environment.f_pps = f_pps;
    m_environment.f_ops = f_ops;
}

void PositroniumProvider::SetThreeGammaModel(
    ThreeGammaModel model
)
{
    m_physics_model.SetThreeGammaModel(model);
}

void PositroniumProvider::SetOrthoThreeGammaFraction(
    double fraction
)
{
    ValidateUnitInterval(
        fraction,
        "OrthoThreeGammaFraction"
    );

    m_environment.ops_3g_fraction = fraction;
    m_environment.ops_2g_fraction = 1.0 - fraction;
}

void PositroniumProvider::SetEnableThreeGamma(
    bool enabled
)
{
    m_physics_model.SetEnableThreeGamma(enabled);

    if (!enabled) {
        m_environment.ops_2g_fraction = 1.0;
        m_environment.ops_3g_fraction = 0.0;
    }
}

void PositroniumProvider::SetDelayMode(
    DelayMode mode
)
{
    m_physics_model.SetDelayMode(mode);
}

void PositroniumProvider::SetTauDirectNs(
    double tau_ns
)
{
    if (tau_ns < 0.0) {
        throw std::invalid_argument(
            "Direct-annihilation lifetime must be non-negative."
        );
    }

    m_environment.tau_direct_ns = tau_ns;
}

void PositroniumProvider::SetTauParaPsNs(
    double tau_ns
)
{
    if (tau_ns <= 0.0) {
        throw std::invalid_argument(
            "Para-Ps lifetime must be strictly positive."
        );
    }

    m_environment.tau_pps_ns = tau_ns;
}

void PositroniumProvider::SetTauOpsNs(
    double tau_ns
)
{
    if (tau_ns <= 0.0) {
        throw std::invalid_argument(
            "Ortho-Ps lifetime must be strictly positive."
        );
    }

    m_environment.tau_ops_ns = tau_ns;
}

void PositroniumProvider::SetFixedDelayNs(
    double fixed_delay_ns
)
{
    m_physics_model.SetFixedDelayNs(
        fixed_delay_ns
    );
}

void PositroniumProvider::SetSourcePosition(
    std::array<double, 3> position_mm
)
{
    m_source_position_mm = position_mm;
}

void PositroniumProvider::SetHasPromptGamma(
    bool enabled
)
{
    m_has_prompt_gamma = enabled;
}

void PositroniumProvider::SetPromptEnergyMeV(
    double energy_MeV
)
{
    if (energy_MeV <= 0.0) {
        throw std::invalid_argument(
            "Prompt gamma energy must be strictly positive."
        );
    }

    m_prompt_energy_MeV = energy_MeV;
}

void PositroniumProvider::SetEnablePositronRange(
    bool enabled
)
{
    m_enable_positron_range = enabled;
}

void PositroniumProvider::SetPositronRangeSigmaMm(
    double sigma_mm
)
{
    if (sigma_mm < 0.0) {
        throw std::invalid_argument(
            "Positron-range sigma must be non-negative."
        );
    }

    m_positron_range_sigma_mm = sigma_mm;
}

void PositroniumProvider::SetEnableQuantumEntanglement(
    bool enabled
)
{
    m_enable_quantum_entanglement = enabled;
}

TimedEventSpec PositroniumProvider::SampleNextEvent()
{
    const PsModelResult model_result =
        m_physics_model.Sample(
            m_environment
        );

    TimedEventSpec event{};

    // Assigned later by PositroniumGenerator.
    event.source_event_id = 0;

    event.ps_class_id =
        static_cast<int>(
            model_result.ps_class
        );

    event.annihilation_mode =
        model_result.annihilation_mode;

    event.delay_ns =
        model_result.delay_ns;

    event.has_prompt_gamma =
        m_has_prompt_gamma;

    event.prompt_energy_MeV =
        m_prompt_energy_MeV;

    event.source_position_mm =
        m_source_position_mm;

    event.medium_id =
        m_environment.medium_id;

    switch (model_result.ps_class) {
        case PsClass::Direct2g:
            event.local_tau_ns =
                m_environment.tau_direct_ns;
            break;

        case PsClass::ParaPs2g:
            event.local_tau_ns =
                m_environment.tau_pps_ns;
            break;

        case PsClass::OrthoPs2g:
        case PsClass::OrthoPs3g:
            event.local_tau_ns =
                m_environment.tau_ops_ns;
            break;
    }

    event.physics_model_name =
        model_result.model_name;

    event.physics_model_version =
        model_result.model_version;

    event.physics_validation_status =
        model_result.validation_status;

    event.qe_mode =
        m_enable_quantum_entanglement &&
        model_result.annihilation_mode == 2;

    std::array<double, 3> annihilation_position =
        m_source_position_mm;

    double positron_range_mm = 0.0;

    if (
        m_enable_positron_range &&
        m_positron_range_sigma_mm > 0.0
    ) {
        const double dx =
            SampleGaussian() *
            m_positron_range_sigma_mm;

        const double dy =
            SampleGaussian() *
            m_positron_range_sigma_mm;

        const double dz =
            SampleGaussian() *
            m_positron_range_sigma_mm;

        annihilation_position[0] += dx;
        annihilation_position[1] += dy;
        annihilation_position[2] += dz;

        positron_range_mm = std::sqrt(
            dx * dx +
            dy * dy +
            dz * dz
        );
    }

    event.annihilation_position_mm =
        annihilation_position;

    event.positron_range_mm =
        positron_range_mm;

    event.vertices.clear();
    event.vertices.reserve(
        m_has_prompt_gamma ? 2 : 1
    );

    if (m_has_prompt_gamma) {
        VertexSpec prompt_vertex{};

        prompt_vertex.vertex_id = 0;
        prompt_vertex.time_ns = 0.0;
        prompt_vertex.position =
            m_source_position_mm;

        ParticleSpec prompt_photon{};

        prompt_photon.pdg_code = 22;
        prompt_photon.kinetic_energy_MeV =
            m_prompt_energy_MeV;

        prompt_photon.direction =
            SampleIsotropicDirection();

        prompt_photon.photon_role =
            static_cast<int>(
                PhotonRole::Prompt
            );

        prompt_photon.parent_vertex_id =
            prompt_vertex.vertex_id;

        prompt_photon.parent_ps_class =
            event.ps_class_id;

        prompt_photon.birth_time_ns = 0.0;

        prompt_vertex.particles.push_back(
            prompt_photon
        );

        event.vertices.push_back(
            prompt_vertex
        );
    }

    VertexSpec annihilation_vertex{};

    annihilation_vertex.vertex_id =
        m_has_prompt_gamma ? 1 : 0;

    annihilation_vertex.time_ns =
        event.delay_ns;

    annihilation_vertex.position =
        annihilation_position;

    annihilation_vertex.particles.reserve(
        model_result.photons.size()
    );

    for (
        std::size_t index = 0;
        index < model_result.photons.size();
        ++index
    ) {
        const PsPhoton& model_photon =
            model_result.photons[index];

        ParticleSpec particle{};

        particle.pdg_code = 22;

        particle.kinetic_energy_MeV =
            model_photon.kinetic_energy_MeV;

        particle.direction =
            model_photon.direction;

        switch (index) {
            case 0:
                particle.photon_role =
                    static_cast<int>(
                        PhotonRole::Ann1
                    );
                break;

            case 1:
                particle.photon_role =
                    static_cast<int>(
                        PhotonRole::Ann2
                    );
                break;

            default:
                particle.photon_role =
                    static_cast<int>(
                        PhotonRole::Ann3
                    );
                break;
        }

        particle.parent_vertex_id =
            annihilation_vertex.vertex_id;

        particle.parent_ps_class =
            event.ps_class_id;

        particle.birth_time_ns =
            event.delay_ns;

        annihilation_vertex.particles.push_back(
            particle
        );
    }

    event.vertices.push_back(
        annihilation_vertex
    );

    return event;
}

void PositroniumProvider::ValidateFractionTriplet(
    double f_direct,
    double f_pps,
    double f_ops
) const
{
    if (
        f_direct < 0.0 ||
        f_pps < 0.0 ||
        f_ops < 0.0
    ) {
        throw std::invalid_argument(
            "All positronium fractions must be non-negative."
        );
    }

    const double sum =
        f_direct +
        f_pps +
        f_ops;

    if (
        std::abs(sum - 1.0) >
        kFractionTolerance
    ) {
        throw std::invalid_argument(
            "Positronium fractions must sum to 1."
        );
    }
}

void PositroniumProvider::ValidateUnitInterval(
    double value,
    const char* name
) const
{
    if (
        value < 0.0 ||
        value > 1.0
    ) {
        throw std::invalid_argument(
            std::string(name) +
            " must be in [0,1]."
        );
    }
}

std::array<double, 3>
PositroniumProvider::SampleIsotropicDirection() const
{
    const double cos_theta =
        -1.0 + 2.0 * G4UniformRand();

    const double sin_theta = std::sqrt(
        std::max(
            0.0,
            1.0 -
            cos_theta * cos_theta
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

double PositroniumProvider::SampleGaussian() const
{
    const double sample1 =
        SampleUniformOpen();

    const double sample2 =
        G4UniformRand();

    return std::sqrt(
        -2.0 * std::log(sample1)
    ) *
    std::cos(
        2.0 * kPi * sample2
    );
}

double PositroniumProvider::SampleUniformOpen() const
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
