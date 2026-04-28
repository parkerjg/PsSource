#include "PositroniumGenerator.hh"
#include "PositroniumTruthInfo.hh"

#include "G4Box.hh"
#include "G4EmLivermorePolarizedPhysics.hh"
#include "G4EmParameters.hh"
#include "G4Event.hh"
#include "G4Exception.hh"
#include "G4ExceptionSeverity.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4PhysListFactory.hh"
#include "G4Run.hh"
#include "G4RunManagerFactory.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4UImanager.hh"
#include "G4UserEventAction.hh"
#include "G4UserRunAction.hh"
#include "G4UserTrackingAction.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VProcess.hh"
#include "G4VUserActionInitialization.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4ios.hh"
#include "Randomize.hh"
#include "CLHEP/Random/RandGauss.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
namespace {

constexpr double kPi = 3.14159265358979323846;

static std::string JsonEscape(const std::string& s)
{
    std::ostringstream os;
    for (char c : s) {
        switch (c) {
            case '\"': os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\n': os << "\\n"; break;
            case '\r': os << "\\r"; break;
            case '\t': os << "\\t"; break;
            default:   os << c; break;
        }
    }
    return os.str();
}

static const char* JsonBool(bool v) { return v ? "true" : "false"; }

static bool ParseBool(const std::string& text)
{
    if (text == "1" || text == "true" || text == "TRUE" || text == "on" || text == "ON") return true;
    if (text == "0" || text == "false" || text == "FALSE" || text == "off" || text == "OFF") return false;
    throw std::runtime_error("Invalid boolean value: " + text);
}

static int ParseInt(const std::string& text, const std::string& label)
{
    try {
        std::size_t idx = 0;
        int v = std::stoi(text, &idx);
        if (idx != text.size()) throw std::runtime_error("");
        return v;
    } catch (...) {
        throw std::runtime_error("Invalid integer for " + label + ": " + text);
    }
}

static double ParseDouble(const std::string& text, const std::string& label)
{
    try {
        std::size_t idx = 0;
        double v = std::stod(text, &idx);
        if (idx != text.size()) throw std::runtime_error("");
        return v;
    } catch (...) {
        throw std::runtime_error("Invalid float for " + label + ": " + text);
    }
}

static std::string SafeProcessName(const G4VProcess* process)
{
    return process ? process->GetProcessName() : "PRIMARY";
}

static bool IsNativeAnnihilationGammaTrack(const G4Track* track)
{
    if (!track) return false;
    if (track->GetParticleDefinition()->GetPDGEncoding() != 22) return false;

    const auto* creator = track->GetCreatorProcess();
    if (!creator) return false;

    const std::string process_name = creator->GetProcessName();

    if (process_name == "annihil") return true;
    if (process_name.find("annihil") != std::string::npos) return true;
    if (process_name.find("Annihil") != std::string::npos) return true;
    if (process_name.find("OrePowell") != std::string::npos) return true;
    if (process_name.find("OrePawell") != std::string::npos) return true;
    if (process_name.find("orepowell") != std::string::npos) return true;
    if (process_name.find("orepawell") != std::string::npos) return true;
    if (process_name.find("3Gamma") != std::string::npos) return true;
    if (process_name.find("3gamma") != std::string::npos) return true;

    return false;
}

static void ApplyRequiredCommand(const G4String& cmd)
{
    auto* ui = G4UImanager::GetUIpointer();
    if (!ui) {
        G4Exception("ApplyRequiredCommand",
                    "PointSource_001",
                    FatalException,
                    "G4UImanager::GetUIpointer() returned null.");
    }

    const G4int rc = ui->ApplyCommand(cmd);
    if (rc != 0) {
        G4ExceptionDescription msg;
        msg << "Failed UI command:\n  " << cmd << "\nReturn code = " << rc;
        G4Exception("ApplyRequiredCommand",
                    "PointSource_002",
                    FatalException,
                    msg);
    }
}

static bool ApplyOptionalCommand(const G4String& cmd)
{
    auto* ui = G4UImanager::GetUIpointer();
    if (!ui) return false;

    const G4int rc = ui->ApplyCommand(cmd);
    if (rc == 0) {
        G4cout << "[ps_pointsource] Applied optional command: " << cmd << G4endl;
        return true;
    }

    G4cout << "[ps_pointsource] Optional command rejected (rc=" << rc << "): " << cmd << G4endl;
    return false;
}

static double ComputeFWHM(const std::vector<double>& x, const std::vector<double>& y)
{
    if (x.size() != y.size() || x.empty()) return std::numeric_limits<double>::quiet_NaN();

    auto it = std::max_element(y.begin(), y.end());
    if (it == y.end() || *it <= 0.0) return std::numeric_limits<double>::quiet_NaN();

    const double halfmax = 0.5 * (*it);

    int i_first = -1;
    int i_last  = -1;
    for (int i = 0; i < static_cast<int>(y.size()); ++i) {
        if (y[i] >= halfmax) {
            if (i_first < 0) i_first = i;
            i_last = i;
        }
    }

    if (i_first < 0 || i_last < 0) return std::numeric_limits<double>::quiet_NaN();

    auto interp = [&](int i0, int i1) -> double {
        if (i0 < 0) i0 = 0;
        if (i1 >= static_cast<int>(x.size())) i1 = static_cast<int>(x.size()) - 1;
        if (i0 == i1) return x[i0];

        const double x0 = x[i0], x1 = x[i1];
        const double y0 = y[i0], y1 = y[i1];
        if (std::abs(y1 - y0) < 1e-12) return 0.5 * (x0 + x1);

        return x0 + (halfmax - y0) * (x1 - x0) / (y1 - y0);
    };

    const double xl = interp(std::max(0, i_first - 1), i_first);
    const double xr = interp(i_last, std::min(static_cast<int>(x.size()) - 1, i_last + 1));

    return xr - xl;
}

} // namespace

// -----------------------------------------------------------------------------
// Options
// -----------------------------------------------------------------------------
enum class GenerationModeChoice { Native, Explicit };
enum class DelayModeChoice { Exponential, Fixed };
enum class ReconMethodChoice { LOR2G, CONE3G, UNIFIED };

struct AppOptions {
    int beam_on = 100000;
    bool beam_on_specified = false;

    std::string preset_name;
    GenerationModeChoice generation_mode = GenerationModeChoice::Explicit;

    bool enable_qe = true;
    bool enable_3gamma_onfly = false;
    std::string positron_at_rest_model = "OrePowellPolar";
    bool set_orto_ps_fraction = false;
    double orto_ps_fraction = 0.0;
    std::string world_material = "G4_AIR";

    double f_direct = 0.0;
    double f_pps = 0.0;
    double f_ops = 1.0;
    double ortho_3g_fraction = 0.0;

    DelayModeChoice delay_mode = DelayModeChoice::Exponential;
    double tau_pps_ns = 0.125;
    double tau_ops_ns = 3.0;
    double fixed_delay_ns = 3.0;

    bool enable_positron_range = false;
    double positron_range_sigma_mm = 1.0;

    bool has_prompt_gamma = false;
    double prompt_energy_mev = 1.274;

    double positron_kinetic_kev = 0.0001;
    std::array<double, 3> source_mm = {0.0, 0.0, 0.0};

    bool use_external_base_time = false;
    double base_time_ns = 0.0;

    std::vector<std::string> pre_commands;
    std::vector<std::string> post_commands;

    ReconMethodChoice recon_method = ReconMethodChoice::UNIFIED;

    double head_size_mm = 25.0;
    double head_separation_mm = 30.0;
    double head_thickness_mm = 0.5;

    int nx = 101;
    int ny = 101;
    int nz = 1;
    double voxel_size_mm = 0.25;

    double sigma_hit_mm = 0.25;     // detector-plane hit-position blur
    double sigma_recon_mm = 0.50;   // Gaussian deposition blur into image

    bool ideal_acceptance = true;
    bool write_pgm = true;
};

static const char* GenerationModeName(GenerationModeChoice m)
{
    return (m == GenerationModeChoice::Native) ? "native" : "explicit";
}

static const char* DelayModeName(DelayModeChoice m)
{
    return (m == DelayModeChoice::Exponential) ? "exponential" : "fixed";
}

static const char* ReconMethodName(ReconMethodChoice m)
{
    switch (m) {
        case ReconMethodChoice::LOR2G:   return "lor2g";
        case ReconMethodChoice::CONE3G:  return "cone3g";
        case ReconMethodChoice::UNIFIED: return "unified";
        default: return "unknown";
    }
}

static ReconMethodChoice ParseReconMethod(const std::string& text)
{
    if (text == "lor2g")   return ReconMethodChoice::LOR2G;
    if (text == "cone3g")  return ReconMethodChoice::CONE3G;
    if (text == "unified") return ReconMethodChoice::UNIFIED;
    throw std::runtime_error("Invalid --recon-method: " + text + " (allowed: lor2g, cone3g, unified)");
}

static GenerationModeChoice ParseGenerationMode(const std::string& text)
{
    if (text == "native" || text == "Native") return GenerationModeChoice::Native;
    if (text == "explicit" || text == "Explicit") return GenerationModeChoice::Explicit;
    throw std::runtime_error("Invalid --generation-mode: " + text);
}

static DelayModeChoice ParseDelayMode(const std::string& text)
{
    if (text == "exponential" || text == "Exponential") return DelayModeChoice::Exponential;
    if (text == "fixed" || text == "Fixed") return DelayModeChoice::Fixed;
    throw std::runtime_error("Invalid --delay-mode: " + text);
}

static void ValidatePositronAtRestModel(const std::string& model)
{
    if (model == "Simple") return;
    if (model == "Allison") return;
    if (model == "OrePawell") return;
    if (model == "OrePowellPolar") return;
    throw std::runtime_error("Invalid --at-rest-model: " + model);
}

static void ValidateFractionTriplet(double f_direct, double f_pps, double f_ops)
{
    constexpr double tol = 1e-9;
    if (f_direct < 0.0 || f_pps < 0.0 || f_ops < 0.0) {
        throw std::runtime_error("Fractions must be non-negative.");
    }
    const double sum = f_direct + f_pps + f_ops;
    if (std::abs(sum - 1.0) > tol) {
        throw std::runtime_error("f_direct + f_pps + f_ops must sum to 1.0");
    }
}

static std::string FindPresetName(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--preset") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --preset");
            return argv[i + 1];
        }
    }
    return "";
}

static void ApplyPreset(AppOptions& opt, const std::string& name)
{
    if (name == "native_reference") {
        opt.generation_mode = GenerationModeChoice::Native;
        opt.enable_qe = true;
        opt.enable_3gamma_onfly = false;
        opt.positron_at_rest_model = "OrePowellPolar";
        opt.world_material = "G4_AIR";
        opt.set_orto_ps_fraction = false;
        opt.has_prompt_gamma = false;
        opt.enable_positron_range = false;
    }
    else if (name == "explicit_2g_reference") {
        opt.generation_mode = GenerationModeChoice::Explicit;
        opt.f_direct = 0.0;
        opt.f_pps = 0.0;
        opt.f_ops = 1.0;
        opt.ortho_3g_fraction = 0.0;
        opt.delay_mode = DelayModeChoice::Exponential;
        opt.tau_ops_ns = 3.0;
        opt.has_prompt_gamma = false;
        opt.enable_positron_range = false;
    }
    else if (name == "explicit_3g_dev") {
        opt.generation_mode = GenerationModeChoice::Explicit;
        opt.f_direct = 0.0;
        opt.f_pps = 0.0;
        opt.f_ops = 1.0;
        opt.ortho_3g_fraction = 1.0;
        opt.delay_mode = DelayModeChoice::Exponential;
        opt.tau_ops_ns = 3.0;
        opt.has_prompt_gamma = false;
        opt.enable_positron_range = false;
    }
    else if (name == "explicit_mixed_qepet") {
        opt.generation_mode = GenerationModeChoice::Explicit;
        opt.f_direct = 0.3;
        opt.f_pps = 0.2;
        opt.f_ops = 0.5;
        opt.ortho_3g_fraction = 1.0;
        opt.delay_mode = DelayModeChoice::Exponential;
        opt.tau_pps_ns = 0.125;
        opt.tau_ops_ns = 3.0;
        opt.has_prompt_gamma = true;
        opt.prompt_energy_mev = 1.274;
        opt.enable_positron_range = false;
    }
    else if (name == "explicit_range_prompt") {
        opt.generation_mode = GenerationModeChoice::Explicit;
        opt.f_direct = 0.3;
        opt.f_pps = 0.2;
        opt.f_ops = 0.5;
        opt.ortho_3g_fraction = 1.0;
        opt.delay_mode = DelayModeChoice::Exponential;
        opt.tau_pps_ns = 0.125;
        opt.tau_ops_ns = 3.0;
        opt.has_prompt_gamma = true;
        opt.prompt_energy_mev = 1.274;
        opt.enable_positron_range = true;
        opt.positron_range_sigma_mm = 1.0;
    }
    else {
        throw std::runtime_error("Unknown preset: " + name);
    }
    opt.preset_name = name;
}

static AppOptions ParseCommandLine(int argc, char** argv, AppOptions opt = AppOptions{})
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--preset") {
            ++i;
        }
        else if (arg == "--beam-on") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --beam-on");
            opt.beam_on = ParseInt(argv[++i], "--beam-on");
            opt.beam_on_specified = true;
        }
        else if (arg == "--generation-mode") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --generation-mode");
            opt.generation_mode = ParseGenerationMode(argv[++i]);
        }
        else if (arg == "--qe") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --qe");
            opt.enable_qe = ParseBool(argv[++i]);
        }
        else if (arg == "--3gamma-onfly") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --3gamma-onfly");
            opt.enable_3gamma_onfly = ParseBool(argv[++i]);
        }
        else if (arg == "--at-rest-model") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --at-rest-model");
            opt.positron_at_rest_model = argv[++i];
            ValidatePositronAtRestModel(opt.positron_at_rest_model);
        }
        else if (arg == "--world-material") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --world-material");
            opt.world_material = argv[++i];
        }
        else if (arg == "--orto-ps-fraction") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --orto-ps-fraction");
            opt.orto_ps_fraction = ParseDouble(argv[++i], "--orto-ps-fraction");
            opt.set_orto_ps_fraction = true;
        }
        else if (arg == "--f-direct") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --f-direct");
            opt.f_direct = ParseDouble(argv[++i], "--f-direct");
        }
        else if (arg == "--f-pps") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --f-pps");
            opt.f_pps = ParseDouble(argv[++i], "--f-pps");
        }
        else if (arg == "--f-ops") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --f-ops");
            opt.f_ops = ParseDouble(argv[++i], "--f-ops");
        }
        else if (arg == "--ortho-3g-fraction") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --ortho-3g-fraction");
            opt.ortho_3g_fraction = ParseDouble(argv[++i], "--ortho-3g-fraction");
        }
        else if (arg == "--delay-mode") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --delay-mode");
            opt.delay_mode = ParseDelayMode(argv[++i]);
        }
        else if (arg == "--tau-pps-ns") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --tau-pps-ns");
            opt.tau_pps_ns = ParseDouble(argv[++i], "--tau-pps-ns");
        }
        else if (arg == "--tau-ops-ns") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --tau-ops-ns");
            opt.tau_ops_ns = ParseDouble(argv[++i], "--tau-ops-ns");
        }
        else if (arg == "--fixed-delay-ns") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --fixed-delay-ns");
            opt.fixed_delay_ns = ParseDouble(argv[++i], "--fixed-delay-ns");
        }
        else if (arg == "--positron-range") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --positron-range");
            opt.enable_positron_range = ParseBool(argv[++i]);
        }
        else if (arg == "--positron-range-sigma-mm") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --positron-range-sigma-mm");
            opt.positron_range_sigma_mm = ParseDouble(argv[++i], "--positron-range-sigma-mm");
        }
        else if (arg == "--prompt") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --prompt");
            opt.has_prompt_gamma = ParseBool(argv[++i]);
        }
        else if (arg == "--prompt-energy-mev") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --prompt-energy-mev");
            opt.prompt_energy_mev = ParseDouble(argv[++i], "--prompt-energy-mev");
        }
        else if (arg == "--positron-kev") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --positron-kev");
            opt.positron_kinetic_kev = ParseDouble(argv[++i], "--positron-kev");
        }
        else if (arg == "--source-mm") {
            if (i + 3 >= argc) throw std::runtime_error("Missing values for --source-mm");
            opt.source_mm[0] = ParseDouble(argv[++i], "--source-mm x");
            opt.source_mm[1] = ParseDouble(argv[++i], "--source-mm y");
            opt.source_mm[2] = ParseDouble(argv[++i], "--source-mm z");
        }
        else if (arg == "--base-time-ns") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --base-time-ns");
            opt.base_time_ns = ParseDouble(argv[++i], "--base-time-ns");
        }
        else if (arg == "--use-external-base-time") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --use-external-base-time");
            opt.use_external_base_time = ParseBool(argv[++i]);
        }
        else if (arg == "--recon-method") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --recon-method");
            opt.recon_method = ParseReconMethod(argv[++i]);
        }
        else if (arg == "--head-size-mm") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --head-size-mm");
            opt.head_size_mm = ParseDouble(argv[++i], "--head-size-mm");
        }
        else if (arg == "--head-separation-mm") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --head-separation-mm");
            opt.head_separation_mm = ParseDouble(argv[++i], "--head-separation-mm");
        }
        else if (arg == "--head-thickness-mm") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --head-thickness-mm");
            opt.head_thickness_mm = ParseDouble(argv[++i], "--head-thickness-mm");
        }
        else if (arg == "--nx") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --nx");
            opt.nx = ParseInt(argv[++i], "--nx");
        }
        else if (arg == "--ny") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --ny");
            opt.ny = ParseInt(argv[++i], "--ny");
        }
        else if (arg == "--nz") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --nz");
            opt.nz = ParseInt(argv[++i], "--nz");
        }
        else if (arg == "--voxel-size-mm") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --voxel-size-mm");
            opt.voxel_size_mm = ParseDouble(argv[++i], "--voxel-size-mm");
        }
        else if (arg == "--sigma-hit-mm") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --sigma-hit-mm");
            opt.sigma_hit_mm = ParseDouble(argv[++i], "--sigma-hit-mm");
        }
        else if (arg == "--sigma-recon-mm") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --sigma-recon-mm");
            opt.sigma_recon_mm = ParseDouble(argv[++i], "--sigma-recon-mm");
        }
        else if (arg == "--ideal-acceptance") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --ideal-acceptance");
            opt.ideal_acceptance = ParseBool(argv[++i]);
        }
        else if (arg == "--write-pgm") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --write-pgm");
            opt.write_pgm = ParseBool(argv[++i]);
        }
        else if (arg == "--pre-cmd") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --pre-cmd");
            opt.pre_commands.emplace_back(argv[++i]);
        }
        else if (arg == "--post-cmd") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --post-cmd");
            opt.post_commands.emplace_back(argv[++i]);
        }
        else if (arg == "--help" || arg == "-h") {
            G4cout
                << "Usage: ps_pointsource [options]\n"
                << "  --preset NAME\n"
                << "  --beam-on N\n"
                << "  --generation-mode native|explicit\n"
                << "  --f-direct F --f-pps F --f-ops F --ortho-3g-fraction F\n"
                << "  --prompt on|off --positron-range on|off\n"
                << "  --recon-method lor2g|cone3g|unified\n"
                << "  --head-size-mm X --head-separation-mm X\n"
                << "  --nx N --ny N --voxel-size-mm X\n"
                << "  --sigma-hit-mm X --sigma-recon-mm X\n"
                << "  --ideal-acceptance on|off\n";
            std::exit(0);
        }
        else {
            throw std::runtime_error("Unknown option: " + arg);
        }
    }

    ValidateFractionTriplet(opt.f_direct, opt.f_pps, opt.f_ops);

    if (opt.nx <= 0 || opt.ny <= 0 || opt.nz <= 0) {
        throw std::runtime_error("nx, ny, nz must be positive.");
    }
    if (opt.voxel_size_mm <= 0.0) {
        throw std::runtime_error("voxel size must be positive.");
    }
    if (opt.head_size_mm <= 0.0 || opt.head_separation_mm <= 0.0 || opt.head_thickness_mm <= 0.0) {
        throw std::runtime_error("head geometry parameters must be positive.");
    }
    if (opt.sigma_hit_mm < 0.0 || opt.sigma_recon_mm < 0.0) {
        throw std::runtime_error("sigma values must be non-negative.");
    }

    return opt;
}

static void WriteRunConfigJson(const AppOptions& opt, int argc, char** argv)
{
    std::ofstream out("run_config.json");
    if (!out) {
        throw std::runtime_error("Failed to open run_config.json");
    }

    out << "{\n";
    out << "  \"schema_version\": 1,\n";
    out << "  \"preset_name\": ";
    if (opt.preset_name.empty()) out << "null,\n";
    else out << "\"" << JsonEscape(opt.preset_name) << "\",\n";

    out << "  \"argv\": [";
    for (int i = 0; i < argc; ++i) {
        if (i) out << ", ";
        out << "\"" << JsonEscape(argv[i]) << "\"";
    }
    out << "],\n";

    out << "  \"beam_on\": " << opt.beam_on << ",\n";
    out << "  \"generation_mode\": \"" << GenerationModeName(opt.generation_mode) << "\",\n";
    out << "  \"recon_method\": \"" << ReconMethodName(opt.recon_method) << "\",\n";
    out << "  \"delay_mode\": \"" << DelayModeName(opt.delay_mode) << "\",\n";

    out << "  \"f_direct\": " << opt.f_direct << ",\n";
    out << "  \"f_pps\": " << opt.f_pps << ",\n";
    out << "  \"f_ops\": " << opt.f_ops << ",\n";
    out << "  \"ortho_3g_fraction\": " << opt.ortho_3g_fraction << ",\n";

    out << "  \"prompt\": " << JsonBool(opt.has_prompt_gamma) << ",\n";
    out << "  \"prompt_energy_mev\": " << opt.prompt_energy_mev << ",\n";
    out << "  \"positron_range\": " << JsonBool(opt.enable_positron_range) << ",\n";
    out << "  \"positron_range_sigma_mm\": " << opt.positron_range_sigma_mm << ",\n";
    out << "  \"tau_pps_ns\": " << opt.tau_pps_ns << ",\n";
    out << "  \"tau_ops_ns\": " << opt.tau_ops_ns << ",\n";

    out << "  \"head_size_mm\": " << opt.head_size_mm << ",\n";
    out << "  \"head_separation_mm\": " << opt.head_separation_mm << ",\n";
    out << "  \"head_thickness_mm\": " << opt.head_thickness_mm << ",\n";

    out << "  \"nx\": " << opt.nx << ",\n";
    out << "  \"ny\": " << opt.ny << ",\n";
    out << "  \"nz\": " << opt.nz << ",\n";
    out << "  \"voxel_size_mm\": " << opt.voxel_size_mm << ",\n";
    out << "  \"sigma_hit_mm\": " << opt.sigma_hit_mm << ",\n";
    out << "  \"sigma_recon_mm\": " << opt.sigma_recon_mm << ",\n";
    out << "  \"ideal_acceptance\": " << JsonBool(opt.ideal_acceptance) << "\n";
    out << "}\n";
}

// -----------------------------------------------------------------------------
// Minimal geometry
// -----------------------------------------------------------------------------
class PointSourceDetectorConstruction : public G4VUserDetectorConstruction {
public:
    explicit PointSourceDetectorConstruction(const AppOptions& opt)
        : m_opt(opt)
    {
    }

    G4VPhysicalVolume* Construct() override
    {
        auto* nist = G4NistManager::Instance();

        auto* world_mat = nist->FindOrBuildMaterial(m_opt.world_material, true);
        if (!world_mat) {
            G4ExceptionDescription msg;
            msg << "Failed to build/find world material: " << m_opt.world_material;
            G4Exception("PointSourceDetectorConstruction::Construct",
                        "PointSource_003",
                        FatalException,
                        msg);
        }

        auto* det_mat = nist->FindOrBuildMaterial("G4_Si", true);

        const G4double world_half = 0.5 * m;

        auto* solid_world = new G4Box("World", world_half, world_half, world_half);
        auto* logic_world = new G4LogicalVolume(solid_world, world_mat, "World");
        auto* phys_world = new G4PVPlacement(nullptr, G4ThreeVector(), logic_world,
                                             "World", nullptr, false, 0, true);

        const G4double hx = 0.5 * m_opt.head_size_mm * mm;
        const G4double hy = 0.5 * m_opt.head_size_mm * mm;
        const G4double hz = 0.5 * m_opt.head_thickness_mm * mm;
        const G4double zc = 0.5 * m_opt.head_separation_mm * mm;

        auto* solid_plus = new G4Box("HeadPlus", hx, hy, hz);
        auto* logic_plus = new G4LogicalVolume(solid_plus, det_mat, "HeadPlus");
        new G4PVPlacement(nullptr, G4ThreeVector(0, 0, +zc), logic_plus,
                          "HeadPlus", logic_world, false, 0, true);

        auto* solid_minus = new G4Box("HeadMinus", hx, hy, hz);
        auto* logic_minus = new G4LogicalVolume(solid_minus, det_mat, "HeadMinus");
        new G4PVPlacement(nullptr, G4ThreeVector(0, 0, -zc), logic_minus,
                          "HeadMinus", logic_world, false, 0, true);

        return phys_world;
    }

private:
    AppOptions m_opt;
};

// -----------------------------------------------------------------------------
// Truth record
// -----------------------------------------------------------------------------
struct GammaBirthRecord {
    int track_id = -1;
    int parent_id = -1;
    int pdg = 0;
    std::string creator_process = "UNKNOWN";

    double vertex_time_ns = -1.0;
    double vertex_x_mm = 0.0;
    double vertex_y_mm = 0.0;
    double vertex_z_mm = 0.0;

    double dir_x = 0.0;
    double dir_y = 0.0;
    double dir_z = 0.0;

    double kinetic_energy_mev = 0.0;
};

struct DetectedHit {
    int track_id = -1;
    int detector_sign = 0;  // +1 or -1
    double x_mm = 0.0;
    double y_mm = 0.0;
    double z_mm = 0.0;
    double dir_x = 0.0;
    double dir_y = 0.0;
    double dir_z = 0.0;
    double energy_mev = 0.0;
};

struct EventTruthPacket {
    int event_id = -1;
    uint64_t source_event_id = 0;

    bool has_prompt_gamma = false;
    int annihilation_mode = -1;
    bool explicit_truth_available = false;

    double source_x_mm = 0.0;
    double source_y_mm = 0.0;
    double source_z_mm = 0.0;

    double ann_x_mm = std::numeric_limits<double>::quiet_NaN();
    double ann_y_mm = std::numeric_limits<double>::quiet_NaN();
    double ann_z_mm = std::numeric_limits<double>::quiet_NaN();
    double delay_ns = -1.0;
};

// -----------------------------------------------------------------------------
// Analysis manager
// -----------------------------------------------------------------------------
class PointSourceAnalysisManager {
public:
    explicit PointSourceAnalysisManager(const AppOptions& opt)
        : m_opt(opt),
          m_image(static_cast<std::size_t>(opt.nx * opt.ny), 0.0)
    {
        m_half_extent_x_mm = 0.5 * (opt.nx - 1) * opt.voxel_size_mm;
        m_half_extent_y_mm = 0.5 * (opt.ny - 1) * opt.voxel_size_mm;
        m_zdet_plus_mm  = +0.5 * opt.head_separation_mm;
        m_zdet_minus_mm = -0.5 * opt.head_separation_mm;
    }

    void ProcessEvent(const EventTruthPacket& truth,
                      const std::vector<GammaBirthRecord>& gammas)
    {
        ++m_total_events;

        if (truth.annihilation_mode <= 0) {
            return;
        }

        if (truth.annihilation_mode == 2) {
            ++m_mode2_events;
        } else if (truth.annihilation_mode == 3) {
            ++m_mode3_events;
        }

        std::vector<DetectedHit> hits = AnalyticDetect(gammas);

        if (!hits.empty()) {
            ++m_detected_events;
            if (truth.annihilation_mode == 2) ++m_detected_mode2_events;
            if (truth.annihilation_mode == 3) ++m_detected_mode3_events;
        }

        if (hits.empty()) return;

        double xrec = std::numeric_limits<double>::quiet_NaN();
        double yrec = std::numeric_limits<double>::quiet_NaN();

        bool used = false;

        switch (m_opt.recon_method) {
            case ReconMethodChoice::LOR2G:
                used = ReconstructLOR2G(truth, hits, xrec, yrec);
                break;
            case ReconMethodChoice::CONE3G:
                used = ReconstructCONE3G(truth, hits, xrec, yrec);
                break;
            case ReconMethodChoice::UNIFIED:
                if (truth.annihilation_mode == 2) {
                    used = ReconstructLOR2G(truth, hits, xrec, yrec);
                } else if (truth.annihilation_mode == 3) {
                    used = ReconstructCONE3G(truth, hits, xrec, yrec);
                }
                break;
            default:
                used = false;
                break;
        }

        if (!used || !std::isfinite(xrec) || !std::isfinite(yrec)) {
            return;
        }

        DepositGaussian(xrec, yrec);

        ++m_reconstructed_events;
        if (truth.annihilation_mode == 2) ++m_reconstructed_mode2_events;
        if (truth.annihilation_mode == 3) ++m_reconstructed_mode3_events;
    }

    void WriteOutputs() const
    {
        WriteReconCSV();
        if (m_opt.write_pgm) {
            WriteReconPGM();
        }
        WriteProfiles();
        WriteFWHMJson();
        WriteSummaryJson();
    }

private:
    std::vector<DetectedHit> AnalyticDetect(const std::vector<GammaBirthRecord>& gammas) const
    {
        std::vector<DetectedHit> out;
        out.reserve(gammas.size());

        const double half = 0.5 * m_opt.head_size_mm;

        for (const auto& g : gammas) {
            if (g.pdg != 22) continue;
            if (std::abs(g.dir_z) < 1e-12) continue;

            const double zdet = (g.dir_z > 0.0) ? m_zdet_plus_mm : m_zdet_minus_mm;
            const int detsign = (g.dir_z > 0.0) ? +1 : -1;

            const double t = (zdet - g.vertex_z_mm) / g.dir_z;
            if (t <= 0.0) continue;

            double x = g.vertex_x_mm + t * g.dir_x;
            double y = g.vertex_y_mm + t * g.dir_y;

            if (!m_opt.ideal_acceptance) {
                if (std::abs(x) > half || std::abs(y) > half) continue;
            }

            if (m_opt.sigma_hit_mm > 0.0) {
                x += CLHEP::RandGauss::shoot(0.0, m_opt.sigma_hit_mm);
                y += CLHEP::RandGauss::shoot(0.0, m_opt.sigma_hit_mm);
            }

            if (!m_opt.ideal_acceptance) {
                if (std::abs(x) > half || std::abs(y) > half) continue;
            }

            DetectedHit h;
            h.track_id = g.track_id;
            h.detector_sign = detsign;
            h.x_mm = x;
            h.y_mm = y;
            h.z_mm = zdet;
            h.dir_x = g.dir_x;
            h.dir_y = g.dir_y;
            h.dir_z = g.dir_z;
            h.energy_mev = g.kinetic_energy_mev;
            out.push_back(h);
        }

        return out;
    }

    bool ReconstructLOR2G(const EventTruthPacket& truth,
                          const std::vector<DetectedHit>& hits,
                          double& xrec,
                          double& yrec) const
    {
        if (truth.annihilation_mode != 2) return false;
        if (hits.size() < 2) return false;

        int ip = -1, im = -1;
        for (int i = 0; i < static_cast<int>(hits.size()); ++i) {
            if (hits[i].detector_sign > 0 && ip < 0) ip = i;
            if (hits[i].detector_sign < 0 && im < 0) im = i;
        }
        if (ip < 0 || im < 0) return false;

        xrec = 0.5 * (hits[ip].x_mm + hits[im].x_mm);
        yrec = 0.5 * (hits[ip].y_mm + hits[im].y_mm);
        return true;
    }

    bool ReconstructCONE3G(const EventTruthPacket& truth,
                           const std::vector<DetectedHit>& hits,
                           double& xrec,
                           double& yrec) const
    {
        if (truth.annihilation_mode != 3) return false;
        if (hits.size() < 3) return false;

        double sx = 0.0;
        double sy = 0.0;
        int n = 0;

        for (const auto& h : hits) {
            if (std::abs(h.dir_z) < 1e-12) continue;
            const double x0 = h.x_mm - (h.dir_x / h.dir_z) * h.z_mm;
            const double y0 = h.y_mm - (h.dir_y / h.dir_z) * h.z_mm;
            if (std::isfinite(x0) && std::isfinite(y0)) {
                sx += x0;
                sy += y0;
                ++n;
            }
        }

        if (n < 3) return false;

        xrec = sx / n;
        yrec = sy / n;
        return true;
    }

    void DepositGaussian(double xrec_mm, double yrec_mm)
    {
        const double sigma = std::max(1e-9, m_opt.sigma_recon_mm);
        const double two_sigma2 = 2.0 * sigma * sigma;

        const int rad = std::max(2, static_cast<int>(std::ceil(3.0 * sigma / m_opt.voxel_size_mm)));

        const int ix0 = CoordToIndexX(xrec_mm);
        const int iy0 = CoordToIndexY(yrec_mm);

        for (int iy = std::max(0, iy0 - rad); iy <= std::min(m_opt.ny - 1, iy0 + rad); ++iy) {
            const double y = IndexToCoordY(iy);
            for (int ix = std::max(0, ix0 - rad); ix <= std::min(m_opt.nx - 1, ix0 + rad); ++ix) {
                const double x = IndexToCoordX(ix);
                const double dx = x - xrec_mm;
                const double dy = y - yrec_mm;
                const double w = std::exp(-(dx * dx + dy * dy) / two_sigma2);
                m_image[static_cast<std::size_t>(iy * m_opt.nx + ix)] += w;
            }
        }
    }

    int CoordToIndexX(double x_mm) const
    {
        return static_cast<int>(std::lround((x_mm + m_half_extent_x_mm) / m_opt.voxel_size_mm));
    }

    int CoordToIndexY(double y_mm) const
    {
        return static_cast<int>(std::lround((y_mm + m_half_extent_y_mm) / m_opt.voxel_size_mm));
    }

    double IndexToCoordX(int ix) const
    {
        return -m_half_extent_x_mm + ix * m_opt.voxel_size_mm;
    }

    double IndexToCoordY(int iy) const
    {
        return -m_half_extent_y_mm + iy * m_opt.voxel_size_mm;
    }

    void WriteReconCSV() const
    {
        std::ofstream out("recon.csv");
        if (!out) throw std::runtime_error("Failed to open recon.csv");

        out << "ix,iy,x_mm,y_mm,value\n";
        for (int iy = 0; iy < m_opt.ny; ++iy) {
            const double y = IndexToCoordY(iy);
            for (int ix = 0; ix < m_opt.nx; ++ix) {
                const double x = IndexToCoordX(ix);
                const double v = m_image[static_cast<std::size_t>(iy * m_opt.nx + ix)];
                out << ix << "," << iy << ","
                    << std::fixed << std::setprecision(6)
                    << x << "," << y << "," << v << "\n";
            }
        }
    }

    void WriteReconPGM() const
    {
        const double vmax = *std::max_element(m_image.begin(), m_image.end());
        std::ofstream out("recon.pgm");
        if (!out) throw std::runtime_error("Failed to open recon.pgm");

        out << "P2\n";
        out << m_opt.nx << " " << m_opt.ny << "\n";
        out << 65535 << "\n";

        for (int iy = 0; iy < m_opt.ny; ++iy) {
            for (int ix = 0; ix < m_opt.nx; ++ix) {
                double v = m_image[static_cast<std::size_t>(iy * m_opt.nx + ix)];
                int pix = 0;
                if (vmax > 0.0) {
                    pix = static_cast<int>(std::lround(65535.0 * v / vmax));
                }
                out << pix;
                if (ix + 1 < m_opt.nx) out << " ";
            }
            out << "\n";
        }
    }

    void WriteProfiles() const
    {
        const int iy0 = m_opt.ny / 2;
        const int ix0 = m_opt.nx / 2;

        std::ofstream fx("profile_x.csv");
        if (!fx) throw std::runtime_error("Failed to open profile_x.csv");
        fx << "x_mm,value\n";
        for (int ix = 0; ix < m_opt.nx; ++ix) {
            fx << std::fixed << std::setprecision(6)
               << IndexToCoordX(ix) << ","
               << m_image[static_cast<std::size_t>(iy0 * m_opt.nx + ix)] << "\n";
        }

        std::ofstream fy("profile_y.csv");
        if (!fy) throw std::runtime_error("Failed to open profile_y.csv");
        fy << "y_mm,value\n";
        for (int iy = 0; iy < m_opt.ny; ++iy) {
            fy << std::fixed << std::setprecision(6)
               << IndexToCoordY(iy) << ","
               << m_image[static_cast<std::size_t>(iy * m_opt.nx + ix0)] << "\n";
        }
    }

    void WriteFWHMJson() const
    {
        const int iy0 = m_opt.ny / 2;
        const int ix0 = m_opt.nx / 2;

        std::vector<double> xx(m_opt.nx), yx(m_opt.nx);
        for (int ix = 0; ix < m_opt.nx; ++ix) {
            xx[ix] = IndexToCoordX(ix);
            yx[ix] = m_image[static_cast<std::size_t>(iy0 * m_opt.nx + ix)];
        }

        std::vector<double> xy(m_opt.ny), yy(m_opt.ny);
        for (int iy = 0; iy < m_opt.ny; ++iy) {
            xy[iy] = IndexToCoordY(iy);
            yy[iy] = m_image[static_cast<std::size_t>(iy * m_opt.nx + ix0)];
        }

        const double fwhm_x = ComputeFWHM(xx, yx);
        const double fwhm_y = ComputeFWHM(xy, yy);
        const double fwhm_mean =
            (std::isfinite(fwhm_x) && std::isfinite(fwhm_y))
                ? 0.5 * (fwhm_x + fwhm_y)
                : std::numeric_limits<double>::quiet_NaN();

        std::ofstream out("fwhm.json");
        if (!out) throw std::runtime_error("Failed to open fwhm.json");

        out << "{\n";
        out << "  \"fwhm_x_mm\": " << fwhm_x << ",\n";
        out << "  \"fwhm_y_mm\": " << fwhm_y << ",\n";
        out << "  \"fwhm_mean_mm\": " << fwhm_mean << "\n";
        out << "}\n";
    }

    void WriteSummaryJson() const
    {
        std::ofstream out("recon_summary.json");
        if (!out) throw std::runtime_error("Failed to open recon_summary.json");

        out << "{\n";
        out << "  \"total_events\": " << m_total_events << ",\n";
        out << "  \"mode2_events\": " << m_mode2_events << ",\n";
        out << "  \"mode3_events\": " << m_mode3_events << ",\n";
        out << "  \"detected_events\": " << m_detected_events << ",\n";
        out << "  \"detected_mode2_events\": " << m_detected_mode2_events << ",\n";
        out << "  \"detected_mode3_events\": " << m_detected_mode3_events << ",\n";
        out << "  \"reconstructed_events\": " << m_reconstructed_events << ",\n";
        out << "  \"reconstructed_mode2_events\": " << m_reconstructed_mode2_events << ",\n";
        out << "  \"reconstructed_mode3_events\": " << m_reconstructed_mode3_events << ",\n";
        out << "  \"recon_method\": \"" << ReconMethodName(m_opt.recon_method) << "\",\n";
        out << "  \"ideal_acceptance\": " << JsonBool(m_opt.ideal_acceptance) << "\n";
        out << "}\n";
    }

private:
    AppOptions m_opt;
    std::vector<double> m_image;

    double m_half_extent_x_mm = 0.0;
    double m_half_extent_y_mm = 0.0;
    double m_zdet_plus_mm = 0.0;
    double m_zdet_minus_mm = 0.0;

    std::uint64_t m_total_events = 0;
    std::uint64_t m_mode2_events = 0;
    std::uint64_t m_mode3_events = 0;

    std::uint64_t m_detected_events = 0;
    std::uint64_t m_detected_mode2_events = 0;
    std::uint64_t m_detected_mode3_events = 0;

    std::uint64_t m_reconstructed_events = 0;
    std::uint64_t m_reconstructed_mode2_events = 0;
    std::uint64_t m_reconstructed_mode3_events = 0;
};

// -----------------------------------------------------------------------------
// Event action
// -----------------------------------------------------------------------------
class PointSourceEventAction : public G4UserEventAction {
public:
    explicit PointSourceEventAction(PointSourceAnalysisManager* manager)
        : m_manager(manager)
    {
    }

    void BeginOfEventAction(const G4Event* event) override
    {
        m_truth = {};
        m_truth.event_id = event ? event->GetEventID() : -1;
        m_gamma_births.clear();

        if (!event) return;

        auto* truth = dynamic_cast<PositroniumTruthInfo*>(event->GetUserInformation());
        if (!truth) return;

        m_truth.source_event_id = truth->source_event_id;
        m_truth.has_prompt_gamma = truth->has_prompt_gamma;
        m_truth.annihilation_mode = truth->annihilation_mode;
        m_truth.explicit_truth_available =
            (truth->annihilation_mode > 0) &&
            std::isfinite(truth->ann_x_mm) &&
            std::isfinite(truth->ann_y_mm) &&
            std::isfinite(truth->ann_z_mm);

        m_truth.source_x_mm = truth->source_x_mm;
        m_truth.source_y_mm = truth->source_y_mm;
        m_truth.source_z_mm = truth->source_z_mm;

        m_truth.ann_x_mm = truth->ann_x_mm;
        m_truth.ann_y_mm = truth->ann_y_mm;
        m_truth.ann_z_mm = truth->ann_z_mm;
        m_truth.delay_ns = truth->delay_ns;
    }

    void EndOfEventAction(const G4Event*) override
    {
        if (m_manager) {
            m_manager->ProcessEvent(m_truth, m_gamma_births);
        }
    }

    void RecordGammaBirth(const G4Track* track)
    {
        GammaBirthRecord rec;
        rec.track_id = track->GetTrackID();
        rec.parent_id = track->GetParentID();
        rec.pdg = track->GetParticleDefinition()->GetPDGEncoding();
        rec.creator_process = SafeProcessName(track->GetCreatorProcess());

        const auto& vtx = track->GetVertexPosition();
        rec.vertex_x_mm = vtx.x() / mm;
        rec.vertex_y_mm = vtx.y() / mm;
        rec.vertex_z_mm = vtx.z() / mm;
        rec.vertex_time_ns = track->GetGlobalTime() / ns;

        const auto& dir = track->GetMomentumDirection();
        rec.dir_x = dir.x();
        rec.dir_y = dir.y();
        rec.dir_z = dir.z();

        rec.kinetic_energy_mev = track->GetKineticEnergy() / MeV;

        m_gamma_births.push_back(rec);
    }

    bool IsExpectedExplicitPrimaryGamma(const G4Track* track) const
    {
        if (!m_truth.explicit_truth_available) return false;
        if (!track) return false;
        if (track->GetParticleDefinition()->GetPDGEncoding() != 22) return false;
        if (track->GetParentID() != 0) return false;
        if (track->GetCreatorProcess() != nullptr) return false;

        const double t_ns = track->GetGlobalTime() / ns;
        const auto& vtx = track->GetVertexPosition();
        const double x_mm = vtx.x() / mm;
        const double y_mm = vtx.y() / mm;
        const double z_mm = vtx.z() / mm;

        constexpr double kTimeTolNs = 1.0e-6;
        constexpr double kPosTolMm  = 1.0e-6;

        const double e_mev = track->GetKineticEnergy() / MeV;
        constexpr double kEnergyTolMeV = 1.0e-6;

        if (m_truth.has_prompt_gamma) {
            const bool matches_source =
                (std::abs(t_ns - 0.0) <= kTimeTolNs) &&
                (std::abs(x_mm - m_truth.source_x_mm) <= kPosTolMm) &&
                (std::abs(y_mm - m_truth.source_y_mm) <= kPosTolMm) &&
                (std::abs(z_mm - m_truth.source_z_mm) <= kPosTolMm);

            auto* truth = dynamic_cast<const PositroniumTruthInfo*>(
                G4RunManager::GetRunManager()->GetCurrentEvent()->GetUserInformation());

            const double prompt_energy_mev =
                truth ? truth->prompt_energy_MeV : 1.274;

            const bool matches_prompt_energy =
                (std::abs(e_mev - prompt_energy_mev) <= kEnergyTolMeV);

            if (matches_source && matches_prompt_energy) {
                return false;
            }
        }

        return (std::abs(x_mm - m_truth.ann_x_mm) <= kPosTolMm) &&
               (std::abs(y_mm - m_truth.ann_y_mm) <= kPosTolMm) &&
               (std::abs(z_mm - m_truth.ann_z_mm) <= kPosTolMm);
    }

private:
    PointSourceAnalysisManager* m_manager = nullptr;
    EventTruthPacket m_truth;
    std::vector<GammaBirthRecord> m_gamma_births;
};

// -----------------------------------------------------------------------------
// Tracking action
// -----------------------------------------------------------------------------
class PointSourceTrackingAction : public G4UserTrackingAction {
public:
    explicit PointSourceTrackingAction(PointSourceEventAction* event_action)
        : m_event_action(event_action)
    {
    }

    void PreUserTrackingAction(const G4Track* track) override
    {
        if (!m_event_action) return;

        if (IsNativeAnnihilationGammaTrack(track) ||
            m_event_action->IsExpectedExplicitPrimaryGamma(track)) {
            m_event_action->RecordGammaBirth(track);
        }
    }

private:
    PointSourceEventAction* m_event_action = nullptr;
};

// -----------------------------------------------------------------------------
// Run action
// -----------------------------------------------------------------------------
class PointSourceRunAction : public G4UserRunAction {
public:
    explicit PointSourceRunAction(PointSourceAnalysisManager* manager)
        : m_manager(manager)
    {
    }

    void EndOfRunAction(const G4Run*) override
    {
        if (m_manager) {
            m_manager->WriteOutputs();
        }
    }

private:
    PointSourceAnalysisManager* m_manager = nullptr;
};

// -----------------------------------------------------------------------------
// Action initialization
// -----------------------------------------------------------------------------
class PointSourceActionInitialization : public G4VUserActionInitialization {
public:
    PointSourceActionInitialization(const AppOptions& opt,
                                    PointSourceAnalysisManager* manager)
        : m_opt(opt), m_manager(manager)
    {
    }

    void Build() const override
    {
        auto* gen = new PositroniumGenerator();

        if (m_opt.generation_mode == GenerationModeChoice::Explicit) {
            gen->SetGenerationMode(PositroniumGenerator::GenerationMode::ExplicitProvider);
        } else {
            gen->SetGenerationMode(PositroniumGenerator::GenerationMode::NativeGeant4);
        }

        gen->SetSourcePosition(m_opt.source_mm);
        gen->SetPositronKineticEnergyKeV(m_opt.positron_kinetic_kev);
        gen->SetHasPromptGamma(m_opt.has_prompt_gamma);
        if (m_opt.has_prompt_gamma) {
            gen->SetPromptEnergyMeV(m_opt.prompt_energy_mev);
        }
        gen->SetEnableQuantumEntanglement(m_opt.enable_qe);
        gen->SetUseExternalBaseTime(m_opt.use_external_base_time);
        if (m_opt.use_external_base_time) {
            gen->SetBaseTimeNs(m_opt.base_time_ns);
        }

        if (m_opt.generation_mode == GenerationModeChoice::Explicit) {
            gen->SetFractions(m_opt.f_direct, m_opt.f_pps, m_opt.f_ops);
            gen->SetEnableThreeGamma(m_opt.ortho_3g_fraction > 0.0);
            gen->SetOrthoThreeGammaFraction(m_opt.ortho_3g_fraction);

            if (m_opt.delay_mode == DelayModeChoice::Exponential) {
                gen->SetDelayMode(PositroniumProvider::DelayMode::Exponential);
            } else {
                gen->SetDelayMode(PositroniumProvider::DelayMode::Fixed);
            }

            gen->SetTauParaPsNs(m_opt.tau_pps_ns);
            gen->SetTauOpsNs(m_opt.tau_ops_ns);
            gen->SetFixedDelayNs(m_opt.fixed_delay_ns);

            gen->SetEnablePositronRange(m_opt.enable_positron_range);
            gen->SetPositronRangeSigmaMm(m_opt.positron_range_sigma_mm);
        }

        auto* event_action = new PointSourceEventAction(m_manager);
        auto* tracking_action = new PointSourceTrackingAction(event_action);
        auto* run_action = new PointSourceRunAction(m_manager);

        SetUserAction(gen);
        SetUserAction(event_action);
        SetUserAction(tracking_action);
        SetUserAction(run_action);
    }

private:
    AppOptions m_opt;
    PointSourceAnalysisManager* m_manager = nullptr;
};

// -----------------------------------------------------------------------------
// EM configuration
// -----------------------------------------------------------------------------
static void ConfigureEmParameters(const AppOptions& opt)
{
    auto* em = G4EmParameters::Instance();
    if (!em) {
        G4Exception("ConfigureEmParameters",
                    "PointSource_004",
                    FatalException,
                    "G4EmParameters::Instance() returned null.");
    }

    em->SetQuantumEntanglement(opt.enable_qe);
    em->Set3GammaAnnihilationOnFly(opt.enable_3gamma_onfly);

    ApplyRequiredCommand(std::string("/process/em/setPositronAtRestModel ") +
                         opt.positron_at_rest_model);
}

static void ConfigureMaterialPositronium(const AppOptions& opt)
{
    if (opt.generation_mode != GenerationModeChoice::Native) {
        return;
    }

    auto* nist = G4NistManager::Instance();
    auto* mat = nist->FindOrBuildMaterial(opt.world_material, true);
    if (!mat) {
        G4ExceptionDescription msg;
        msg << "Failed to build/find material for positronium setup: " << opt.world_material;
        G4Exception("ConfigureMaterialPositronium",
                    "PointSource_005",
                    FatalException,
                    msg);
    }

    if (opt.set_orto_ps_fraction) {
        ApplyRequiredCommand("/material/g4/ortoPositroniumFraction " +
                             opt.world_material + " " +
                             std::to_string(opt.orto_ps_fraction));
    }
}

// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------
int main(int argc, char** argv)
{
    AppOptions opt;

    try {
        AppOptions base_opt;
        const std::string preset_name = FindPresetName(argc, argv);
        if (!preset_name.empty()) {
            ApplyPreset(base_opt, preset_name);
        }
        opt = ParseCommandLine(argc, argv, base_opt);
        WriteRunConfigJson(opt, argc, argv);
    } catch (const std::exception& e) {
        G4cerr << "Argument error: " << e.what() << G4endl;
        return 1;
    }

    auto* manager = new PointSourceAnalysisManager(opt);

    auto* run_manager = G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial);

    run_manager->SetUserInitialization(new PointSourceDetectorConstruction(opt));

    G4PhysListFactory phys_factory;
    auto* physics = phys_factory.GetReferencePhysList("FTFP_BERT");
    if (!physics) {
        G4Exception("main",
                    "PointSource_006",
                    FatalException,
                    "Failed to create FTFP_BERT.");
    }
    physics->ReplacePhysics(new G4EmLivermorePolarizedPhysics());
    run_manager->SetUserInitialization(physics);

    run_manager->SetUserInitialization(new PointSourceActionInitialization(opt, manager));

    ConfigureEmParameters(opt);
    ConfigureMaterialPositronium(opt);

    for (const auto& cmd : opt.pre_commands) {
        ApplyOptionalCommand(cmd);
    }

    run_manager->Initialize();

    G4cout << "\n=== ps_pointsource ===\n"
           << "Generation mode     : " << GenerationModeName(opt.generation_mode) << "\n"
           << "Recon method        : " << ReconMethodName(opt.recon_method) << "\n"
           << "BeamOn              : " << opt.beam_on << "\n"
           << "Head size (mm)      : " << opt.head_size_mm << "\n"
           << "Head separation(mm) : " << opt.head_separation_mm << "\n"
           << "Voxel size (mm)     : " << opt.voxel_size_mm << "\n"
           << "Grid                : " << opt.nx << " x " << opt.ny << " x " << opt.nz << "\n"
           << "Sigma hit (mm)      : " << opt.sigma_hit_mm << "\n"
           << "Sigma recon (mm)    : " << opt.sigma_recon_mm << "\n"
           << "Ideal acceptance    : " << (opt.ideal_acceptance ? "ON" : "OFF") << "\n"
           << "Outputs             : recon.csv, recon.pgm, profile_x.csv, profile_y.csv, fwhm.json, recon_summary.json\n"
           << G4endl;

    for (const auto& cmd : opt.post_commands) {
        ApplyOptionalCommand(cmd);
    }

    ApplyRequiredCommand(std::string("/run/beamOn ") + std::to_string(opt.beam_on));

    delete run_manager;
    delete manager;
    return 0;
}
