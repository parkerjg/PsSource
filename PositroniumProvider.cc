#include "PositroniumProvider.hh"

#include "Randomize.hh"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {
constexpr double kElectronMassMeV = 0.510999;
constexpr double kTwoGammaEnergyMeV = kElectronMassMeV;
constexpr double kThreeGammaTotalEnergyMeV = 2.0 * kElectronMassMeV;
constexpr double kParaPsDefaultTauNs = 0.125;
constexpr double kOpsDefaultTauNs = 3.0;
constexpr double kPi = 3.14159265358979323846;
constexpr double kFractionTol = 1e-9;
constexpr double kMinThreeGammaEnergyMeV = 1.0e-6;

using Vec3 = std::array<double, 3>;

double Dot(const Vec3& a, const Vec3& b)
{
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

Vec3 Cross(const Vec3& a, const Vec3& b)
{
    return {
        a[1]*b[2] - a[2]*b[1],
        a[2]*b[0] - a[0]*b[2],
        a[0]*b[1] - a[1]*b[0]
    };
}

double Norm(const Vec3& v)
{
    return std::sqrt(std::max(0.0, Dot(v, v)));
}

Vec3 Normalize(const Vec3& v)
{
    const double n = Norm(v);
    if (n <= 0.0) {
        return {1.0, 0.0, 0.0};
    }
    return {v[0] / n, v[1] / n, v[2] / n};
}

Vec3 Scale(const Vec3& v, double s)
{
    return {s * v[0], s * v[1], s * v[2]};
}

Vec3 Add(const Vec3& a, const Vec3& b)
{
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}

Vec3 Sub(const Vec3& a, const Vec3& b)
{
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
}  // namespace

PositroniumProvider::PositroniumProvider()
    : m_delay_mode(DelayMode::Exponential),
      m_f_direct(0.3),
      m_f_pps(0.2),
      m_f_ops(0.5),
      m_enable_three_gamma(true),
      m_ortho_three_gamma_fraction(1.0),
      m_tau_pps_ns(kParaPsDefaultTauNs),
      m_tau_ops_ns(kOpsDefaultTauNs),
      m_fixed_delay_ns(kOpsDefaultTauNs),
      m_source_pos({0.0, 0.0, 0.0}),
      m_has_prompt(false),
      m_prompt_energy_MeV(1.274),
      m_enable_positron_range(false),
      m_positron_range_sigma_mm(1.0),
      m_enable_qe(true)
{
    ValidateFractions(m_f_direct, m_f_pps, m_f_ops);
    ValidateUnitInterval(m_ortho_three_gamma_fraction, "OrthoThreeGammaFraction");
}

void PositroniumProvider::SetFractions(double f_direct, double f_pps, double f_ops)
{
    ValidateFractions(f_direct, f_pps, f_ops);
    m_f_direct = f_direct;
    m_f_pps = f_pps;
    m_f_ops = f_ops;
}

void PositroniumProvider::SetOrthoThreeGammaFraction(double f)
{
    ValidateUnitInterval(f, "OrthoThreeGammaFraction");
    m_ortho_three_gamma_fraction = f;
}

void PositroniumProvider::SetEnableThreeGamma(bool v)
{
    m_enable_three_gamma = v;
}

void PositroniumProvider::SetDelayMode(DelayMode mode)
{
    m_delay_mode = mode;
}

void PositroniumProvider::SetTauParaPsNs(double tau_ns)
{
    if (tau_ns <= 0.0) {
        throw std::invalid_argument("Para-Ps tau must be strictly positive.");
    }
    m_tau_pps_ns = tau_ns;
}

void PositroniumProvider::SetTauOpsNs(double tau_ns)
{
    if (tau_ns <= 0.0) {
        throw std::invalid_argument("Ortho-Ps tau must be strictly positive.");
    }
    m_tau_ops_ns = tau_ns;
}

void PositroniumProvider::SetFixedDelayNs(double fixed_ns)
{
    if (fixed_ns < 0.0) {
        throw std::invalid_argument("Fixed delay must be non-negative.");
    }
    m_fixed_delay_ns = fixed_ns;
}

void PositroniumProvider::SetSourcePosition(std::array<double, 3> pos)
{
    m_source_pos = pos;
}

void PositroniumProvider::SetHasPromptGamma(bool has_prompt)
{
    m_has_prompt = has_prompt;
}

void PositroniumProvider::SetPromptEnergyMeV(double e)
{
    if (e <= 0.0) {
        throw std::invalid_argument("Prompt gamma energy must be strictly positive.");
    }
    m_prompt_energy_MeV = e;
}

void PositroniumProvider::SetEnablePositronRange(bool v)
{
    m_enable_positron_range = v;
}

void PositroniumProvider::SetPositronRangeSigmaMm(double s)
{
    if (s < 0.0) {
        throw std::invalid_argument("Positron range sigma must be non-negative.");
    }
    m_positron_range_sigma_mm = s;
}

void PositroniumProvider::SetEnableQuantumEntanglement(bool v)
{
    m_enable_qe = v;
}

TimedEventSpec PositroniumProvider::SampleNextEvent()
{
    TimedEventSpec event{};
    event.source_event_id = 0;  // filled later by PositroniumGenerator

    const PsClass ps_class = SamplePsClass();
    event.ps_class_id = static_cast<int>(ps_class);
    event.delay_ns = SampleDelayNs(ps_class);

    event.has_prompt_gamma = m_has_prompt;
    event.prompt_energy_MeV = m_prompt_energy_MeV;
    event.source_position_mm = m_source_pos;
    event.medium_id = 0;

    event.annihilation_mode =
        (ps_class == PsClass::OrthoPs3g) ? 3 : 2;

    // Metadata only. Treat QE as relevant only for 2-gamma branches.
    event.qe_mode = (m_enable_qe && event.annihilation_mode == 2);

    if (ps_class == PsClass::ParaPs2g) {
        event.local_tau_ns = m_tau_pps_ns;
    } else if (ps_class == PsClass::OrthoPs2g || ps_class == PsClass::OrthoPs3g) {
        event.local_tau_ns = m_tau_ops_ns;
    } else {
        event.local_tau_ns = 0.0;
    }

    // Optional positron-range displacement
    std::array<double, 3> ann_pos = m_source_pos;
    double actual_range_mm = 0.0;

    if (m_enable_positron_range && m_positron_range_sigma_mm > 0.0) {
        const double dx = SampleGaussian() * m_positron_range_sigma_mm;
        const double dy = SampleGaussian() * m_positron_range_sigma_mm;
        const double dz = SampleGaussian() * m_positron_range_sigma_mm;

        ann_pos[0] += dx;
        ann_pos[1] += dy;
        ann_pos[2] += dz;

        actual_range_mm = std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    event.annihilation_position_mm = ann_pos;
    event.positron_range_mm = actual_range_mm;

    event.vertices.clear();
    event.vertices.reserve(m_has_prompt ? 2 : 1);

    // Prompt gamma vertex at source position and t = 0
    if (m_has_prompt) {
        VertexSpec prompt_vtx{};
        prompt_vtx.vertex_id = 0;
        prompt_vtx.time_ns = 0.0;
        prompt_vtx.position = m_source_pos;
        prompt_vtx.particles.reserve(1);

        ParticleSpec pg{};
        pg.pdg_code = 22;
        pg.kinetic_energy_MeV = m_prompt_energy_MeV;
        pg.direction = SampleIsotropicDirection();
        pg.photon_role = static_cast<int>(PhotonRole::Prompt);
        pg.parent_vertex_id = prompt_vtx.vertex_id;
        pg.parent_ps_class = event.ps_class_id;
        pg.birth_time_ns = 0.0;

        prompt_vtx.particles.push_back(pg);
        event.vertices.push_back(prompt_vtx);
    }

    // Annihilation vertex at displaced position and delayed time
    VertexSpec annih_vtx{};
    annih_vtx.vertex_id = 1;
    annih_vtx.time_ns = event.delay_ns;
    annih_vtx.position = ann_pos;
    annih_vtx.particles.reserve(event.annihilation_mode);

    if (event.annihilation_mode == 2) {
        const auto dirs = SampleBackToBackDirections();

        ParticleSpec g1{};
        g1.pdg_code = 22;
        g1.kinetic_energy_MeV = kTwoGammaEnergyMeV;
        g1.direction = dirs.first;
        g1.photon_role = static_cast<int>(PhotonRole::Ann1);
        g1.parent_vertex_id = annih_vtx.vertex_id;
        g1.parent_ps_class = event.ps_class_id;
        g1.birth_time_ns = event.delay_ns;

        ParticleSpec g2{};
        g2.pdg_code = 22;
        g2.kinetic_energy_MeV = kTwoGammaEnergyMeV;
        g2.direction = dirs.second;
        g2.photon_role = static_cast<int>(PhotonRole::Ann2);
        g2.parent_vertex_id = annih_vtx.vertex_id;
        g2.parent_ps_class = event.ps_class_id;
        g2.birth_time_ns = event.delay_ns;

        annih_vtx.particles.push_back(g1);
        annih_vtx.particles.push_back(g2);
    } else {
        std::array<double, 3> energies{};
        std::array<std::array<double, 3>, 3> directions{};
        SampleThreeGammaKinematics(energies, directions);

        ParticleSpec g1{};
        g1.pdg_code = 22;
        g1.kinetic_energy_MeV = energies[0];
        g1.direction = directions[0];
        g1.photon_role = static_cast<int>(PhotonRole::Ann1);
        g1.parent_vertex_id = annih_vtx.vertex_id;
        g1.parent_ps_class = event.ps_class_id;
        g1.birth_time_ns = event.delay_ns;

        ParticleSpec g2{};
        g2.pdg_code = 22;
        g2.kinetic_energy_MeV = energies[1];
        g2.direction = directions[1];
        g2.photon_role = static_cast<int>(PhotonRole::Ann2);
        g2.parent_vertex_id = annih_vtx.vertex_id;
        g2.parent_ps_class = event.ps_class_id;
        g2.birth_time_ns = event.delay_ns;

        ParticleSpec g3{};
        g3.pdg_code = 22;
        g3.kinetic_energy_MeV = energies[2];
        g3.direction = directions[2];
        g3.photon_role = static_cast<int>(PhotonRole::Ann3);
        g3.parent_vertex_id = annih_vtx.vertex_id;
        g3.parent_ps_class = event.ps_class_id;
        g3.birth_time_ns = event.delay_ns;

        annih_vtx.particles.push_back(g1);
        annih_vtx.particles.push_back(g2);
        annih_vtx.particles.push_back(g3);
    }

    event.vertices.push_back(annih_vtx);

    return event;
}

void PositroniumProvider::ValidateFractions(double f_direct, double f_pps, double f_ops) const
{
    if (f_direct < 0.0 || f_pps < 0.0 || f_ops < 0.0) {
        throw std::invalid_argument("All positronium fractions must be non-negative.");
    }

    const double sum = f_direct + f_pps + f_ops;
    if (std::abs(sum - 1.0) > kFractionTol) {
        throw std::invalid_argument("Fractions must sum to 1.0.");
    }
}

void PositroniumProvider::ValidateUnitInterval(double x, const char* name) const
{
    if (x < 0.0 || x > 1.0) {
        throw std::invalid_argument(std::string(name) + " must be in [0,1].");
    }
}

PsClass PositroniumProvider::SamplePsClass() const
{
    const double r = G4UniformRand();

    if (r < m_f_direct) {
        return PsClass::Direct2g;
    }
    if (r < (m_f_direct + m_f_pps)) {
        return PsClass::ParaPs2g;
    }

    if (m_enable_three_gamma && G4UniformRand() < m_ortho_three_gamma_fraction) {
        return PsClass::OrthoPs3g;
    }

    return PsClass::OrthoPs2g;
}

double PositroniumProvider::SampleDelayNs(PsClass ps_class) const
{
    if (ps_class == PsClass::Direct2g) {
        return 0.0;
    }

    if (m_delay_mode == DelayMode::Fixed) {
        return m_fixed_delay_ns;
    }

    if (ps_class == PsClass::ParaPs2g) {
        return -m_tau_pps_ns * std::log(SampleUniformOpen());
    }

    return -m_tau_ops_ns * std::log(SampleUniformOpen());
}

std::array<double, 3> PositroniumProvider::SampleIsotropicDirection() const
{
    const double cos_theta = -1.0 + 2.0 * G4UniformRand();
    const double sin_theta = std::sqrt(std::max(0.0, 1.0 - cos_theta * cos_theta));
    const double phi = 2.0 * kPi * G4UniformRand();

    return {
        sin_theta * std::cos(phi),
        sin_theta * std::sin(phi),
        cos_theta
    };
}

std::pair<std::array<double, 3>, std::array<double, 3>>
PositroniumProvider::SampleBackToBackDirections() const
{
    const auto d1 = SampleIsotropicDirection();
    const std::array<double, 3> d2 = {-d1[0], -d1[1], -d1[2]};
    return {d1, d2};
}

void PositroniumProvider::SampleThreeGammaKinematics(
    std::array<double, 3>& energies_mev,
    std::array<std::array<double, 3>, 3>& directions
) const
{
    // Sample a valid massless 3-body energy partition at rest.
    // Requirements:
    //   E1 + E2 + E3 = 2 m_e c^2
    //   each Ei > 0
    //   max(Ei) <= total/2 so the momentum triangle closes
    while (true) {
        const double a = -std::log(SampleUniformOpen());
        const double b = -std::log(SampleUniformOpen());
        const double c = -std::log(SampleUniformOpen());

        const double sum = a + b + c;
        if (sum <= 0.0) {
            continue;
        }

        energies_mev[0] = kThreeGammaTotalEnergyMeV * a / sum;
        energies_mev[1] = kThreeGammaTotalEnergyMeV * b / sum;
        energies_mev[2] = kThreeGammaTotalEnergyMeV * c / sum;

        const double emax = std::max(energies_mev[0],
                            std::max(energies_mev[1], energies_mev[2]));

        if (energies_mev[0] <= kMinThreeGammaEnergyMeV ||
            energies_mev[1] <= kMinThreeGammaEnergyMeV ||
            energies_mev[2] <= kMinThreeGammaEnergyMeV) {
            continue;
        }

        if (emax <= 0.5 * kThreeGammaTotalEnergyMeV) {
            break;
        }
    }

    // Construct exact momentum-conserving directions in a random plane.
    const Vec3 plane_normal = SampleIsotropicDirection();

    const Vec3 trial_axis =
        (std::abs(plane_normal[2]) < 0.9) ? Vec3{0.0, 0.0, 1.0}
                                          : Vec3{1.0, 0.0, 0.0};

    const Vec3 u = Normalize(Cross(trial_axis, plane_normal));
    const Vec3 v = Normalize(Cross(plane_normal, u));

    const double e1 = energies_mev[0];
    const double e2 = energies_mev[1];
    const double e3 = energies_mev[2];

    double cos_alpha = (e3 * e3 - e1 * e1 - e2 * e2) / (2.0 * e1 * e2);
    cos_alpha = std::max(-1.0, std::min(1.0, cos_alpha));

    double sin_alpha = std::sqrt(std::max(0.0, 1.0 - cos_alpha * cos_alpha));
    if (G4UniformRand() < 0.5) {
        sin_alpha = -sin_alpha;
    }

    const Vec3 d1 = u;
    const Vec3 d2 = Add(Scale(u, cos_alpha), Scale(v, sin_alpha));

    const Vec3 p1 = Scale(d1, e1);
    const Vec3 p2 = Scale(d2, e2);
    const Vec3 p3 = Scale(Sub({0.0, 0.0, 0.0}, Add(p1, p2)), 1.0 / e3);

    directions[0] = Normalize(d1);
    directions[1] = Normalize(d2);
    directions[2] = Normalize(p3);
}

double PositroniumProvider::SampleGaussian() const
{
    const double u1 = SampleUniformOpen();
    const double u2 = G4UniformRand();

    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * kPi * u2);
}

double PositroniumProvider::SampleUniformOpen() const
{
    double u = G4UniformRand();

    while (u <= 0.0 || u >= 1.0) {
        u = G4UniformRand();
    }

    return u;
}
