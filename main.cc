#include "PositroniumGenerator.hh"

#include "G4Box.hh"
#include "G4EmLivermorePolarizedPhysics.hh"
#include "G4EmParameters.hh"
#include "G4Exception.hh"
#include "G4ExceptionSeverity.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4PhysListFactory.hh"
#include "G4RunManagerFactory.hh"
#include "G4SystemOfUnits.hh"
#include "G4UImanager.hh"
#include "G4VUserActionInitialization.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4ios.hh"

#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Options
// -----------------------------------------------------------------------------
enum class GenerationModeChoice { Native, Explicit };
enum class DelayModeChoice { Exponential, Fixed };
enum class ThreeGammaModelChoice {
    Approximate,
    OrePowell,
    OrePowellPolarized
};

struct AppOptions {
    int beam_on = 10;
    bool beam_on_specified = false;

    std::string preset_name;

    GenerationModeChoice generation_mode = GenerationModeChoice::Native;

    bool enable_qe = true;
    bool enable_3gamma_onfly = false;
    std::string positron_at_rest_model = "OrePowellPolar";

    // Native Geant4 material-side ortho-Ps branch control
    bool set_orto_ps_fraction = false;
    double orto_ps_fraction = 0.0;
    std::string world_material = "G4_AIR";

    ThreeGammaModelChoice three_gamma_model =
        ThreeGammaModelChoice::Approximate;

    // Explicit provider controls
    double f_direct = 0.3;
    double f_pps = 0.2;
    double f_ops = 0.5;

    double ortho_3g_fraction = 1.0;
    DelayModeChoice delay_mode = DelayModeChoice::Exponential;
    double tau_pps_ns = 0.125;
    double tau_ops_ns = 3.0;
    double fixed_delay_ns = 3.0;

    bool enable_positron_range = false;
    double positron_range_sigma_mm = 1.0;

    // Shared source controls
    bool has_prompt_gamma = true;
    double prompt_energy_mev = 1.274;

    double positron_kinetic_kev = 0.0001;
    std::array<double, 3> source_mm = {0.0, 0.0, 0.0};

    bool use_external_base_time = false;
    double base_time_ns = 0.0;

    bool print_em_parameters = false;
    bool no_run = false;
    std::string macro_file;

    std::vector<std::string> pre_commands;
    std::vector<std::string> post_commands;
};

// -----------------------------------------------------------------------------
// Helper naming / preset / JSON helpers
// -----------------------------------------------------------------------------
static const char* GenerationModeName(GenerationModeChoice m)
{
    return (m == GenerationModeChoice::Native) ? "native" : "explicit";
}

static const char* ThreeGammaModelName(
    ThreeGammaModelChoice model
)
{
    switch (model) {
        case ThreeGammaModelChoice::Approximate:
            return "approximate";

        case ThreeGammaModelChoice::OrePowell:
            return "ore-powell";

        case ThreeGammaModelChoice::OrePowellPolarized:
            return "ore-powell-polarized";
    }

    return "unknown";
}

static ThreeGammaModelChoice ParseThreeGammaModel(
    const std::string& text
)
{
    if (text == "approximate") {
        return ThreeGammaModelChoice::Approximate;
    }

    if (text == "ore-powell") {
        return ThreeGammaModelChoice::OrePowell;
    }

    if (text == "ore-powell-polarized") {
        return ThreeGammaModelChoice::OrePowellPolarized;
    }

    throw std::runtime_error(
        "Invalid --three-gamma-model: " + text +
        ". Allowed: approximate, ore-powell, "
        "ore-powell-polarized"
    );
}

static const char* DelayModeName(DelayModeChoice m)
{
    return (m == DelayModeChoice::Exponential) ? "exponential" : "fixed";
}

static std::string FindPresetName(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--preset") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for --preset");
            }
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
        opt.has_prompt_gamma = true;
        opt.prompt_energy_mev = 1.274;
        opt.enable_positron_range = false;
    }
    else if (name == "native_ortho_half") {
        opt.generation_mode = GenerationModeChoice::Native;
        opt.enable_qe = true;
        opt.enable_3gamma_onfly = false;
        opt.positron_at_rest_model = "OrePowellPolar";
        opt.world_material = "G4_AIR";
        opt.set_orto_ps_fraction = true;
        opt.orto_ps_fraction = 0.50;
        opt.has_prompt_gamma = true;
        opt.prompt_energy_mev = 1.274;
        opt.enable_positron_range = false;
    }
    else if (name == "explicit_3g_dev") {
        opt.generation_mode = GenerationModeChoice::Explicit;
        opt.enable_qe = true;
        opt.f_direct = 0.0;
        opt.f_pps = 0.0;
        opt.f_ops = 1.0;
        opt.ortho_3g_fraction = 1.0;
        opt.delay_mode = DelayModeChoice::Exponential;
        opt.tau_pps_ns = 0.125;
        opt.tau_ops_ns = 3.0;
        opt.fixed_delay_ns = 3.0;
        opt.has_prompt_gamma = false;
        opt.enable_positron_range = false;
    }
    else if (name == "explicit_2g_reference") {
        opt.generation_mode = GenerationModeChoice::Explicit;
        opt.enable_qe = true;
        opt.f_direct = 0.3;
        opt.f_pps = 0.2;
        opt.f_ops = 0.5;
        opt.ortho_3g_fraction = 0.0;
        opt.delay_mode = DelayModeChoice::Exponential;
        opt.tau_pps_ns = 0.125;
        opt.tau_ops_ns = 3.0;
        opt.fixed_delay_ns = 3.0;
        opt.has_prompt_gamma = false;
        opt.enable_positron_range = false;
    }
    else if (name == "explicit_mixed_qepet") {
        opt.generation_mode = GenerationModeChoice::Explicit;
        opt.enable_qe = true;
        opt.f_direct = 0.3;
        opt.f_pps = 0.2;
        opt.f_ops = 0.5;
        opt.ortho_3g_fraction = 1.0;
        opt.delay_mode = DelayModeChoice::Exponential;
        opt.tau_pps_ns = 0.125;
        opt.tau_ops_ns = 3.0;
        opt.fixed_delay_ns = 3.0;
        opt.has_prompt_gamma = true;
        opt.prompt_energy_mev = 1.274;
        opt.enable_positron_range = false;
    }
    else if (name == "explicit_range_prompt") {
        opt.generation_mode = GenerationModeChoice::Explicit;
        opt.enable_qe = true;
        opt.f_direct = 0.3;
        opt.f_pps = 0.2;
        opt.f_ops = 0.5;
        opt.ortho_3g_fraction = 1.0;
        opt.delay_mode = DelayModeChoice::Exponential;
        opt.tau_pps_ns = 0.125;
        opt.tau_ops_ns = 3.0;
        opt.fixed_delay_ns = 3.0;
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

static const char* JsonBool(bool v)
{
    return v ? "true" : "false";
}

static void WriteStringArrayJson(std::ofstream& out,
                                 const char* key,
                                 const std::vector<std::string>& values,
                                 bool trailing_comma = true)
{
    out << "  \"" << key << "\": [";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ", ";
        out << "\"" << JsonEscape(values[i]) << "\"";
    }
    out << "]";
    if (trailing_comma) out << ",";
    out << "\n";
}

static void WriteArgvJson(std::ofstream& out, int argc, char** argv)
{
    out << "  \"argv\": [";
    for (int i = 0; i < argc; ++i) {
        if (i) out << ", ";
        out << "\"" << JsonEscape(argv[i]) << "\"";
    }
    out << "],\n";
}

static void WriteRunConfigJson(const AppOptions& opt,
                               int argc,
                               char** argv,
                               const std::string& filename = "run_config.json")
{
    std::ofstream out(filename);
    if (!out) {
        throw std::runtime_error("Failed to open " + filename + " for writing.");
    }

    out << "{\n";
    out << "  \"schema_version\": 1,\n";

    out << "  \"preset_name\": ";
    if (opt.preset_name.empty()) out << "null,\n";
    else out << "\"" << JsonEscape(opt.preset_name) << "\",\n";

    WriteArgvJson(out, argc, argv);

    out << "  \"generation_mode\": \"" << GenerationModeName(opt.generation_mode) << "\",\n";
    out << "  \"delay_mode\": \"" << DelayModeName(opt.delay_mode) << "\",\n";

    out << "  \"beam_on\": " << opt.beam_on << ",\n";
    out << "  \"beam_on_specified\": " << JsonBool(opt.beam_on_specified) << ",\n";

    out << "  \"enable_qe\": " << JsonBool(opt.enable_qe) << ",\n";
    out << "  \"three_gamma_model\": \""
        << ThreeGammaModelName(opt.three_gamma_model)
        << "\",\n";
    out << "  \"enable_3gamma_onfly\": " << JsonBool(opt.enable_3gamma_onfly) << ",\n";
    out << "  \"positron_at_rest_model\": \"" << JsonEscape(opt.positron_at_rest_model) << "\",\n";
    out << "  \"world_material\": \"" << JsonEscape(opt.world_material) << "\",\n";

    out << "  \"set_orto_ps_fraction\": " << JsonBool(opt.set_orto_ps_fraction) << ",\n";
    out << "  \"orto_ps_fraction\": " << opt.orto_ps_fraction << ",\n";

    out << "  \"f_direct\": " << opt.f_direct << ",\n";
    out << "  \"f_pps\": " << opt.f_pps << ",\n";
    out << "  \"f_ops\": " << opt.f_ops << ",\n";
    out << "  \"ortho_3g_fraction\": " << opt.ortho_3g_fraction << ",\n";

    out << "  \"tau_pps_ns\": " << opt.tau_pps_ns << ",\n";
    out << "  \"tau_ops_ns\": " << opt.tau_ops_ns << ",\n";
    out << "  \"fixed_delay_ns\": " << opt.fixed_delay_ns << ",\n";

    out << "  \"enable_positron_range\": " << JsonBool(opt.enable_positron_range) << ",\n";
    out << "  \"positron_range_sigma_mm\": " << opt.positron_range_sigma_mm << ",\n";

    out << "  \"has_prompt_gamma\": " << JsonBool(opt.has_prompt_gamma) << ",\n";
    out << "  \"prompt_energy_mev\": " << opt.prompt_energy_mev << ",\n";

    out << "  \"positron_kinetic_kev\": " << opt.positron_kinetic_kev << ",\n";
    out << "  \"source_mm\": [" << opt.source_mm[0] << ", " << opt.source_mm[1] << ", " << opt.source_mm[2] << "],\n";

    out << "  \"use_external_base_time\": " << JsonBool(opt.use_external_base_time) << ",\n";
    out << "  \"base_time_ns\": " << opt.base_time_ns << ",\n";

    out << "  \"print_em_parameters\": " << JsonBool(opt.print_em_parameters) << ",\n";
    out << "  \"no_run\": " << JsonBool(opt.no_run) << ",\n";

    out << "  \"macro_file\": ";
    if (opt.macro_file.empty()) out << "null,\n";
    else out << "\"" << JsonEscape(opt.macro_file) << "\",\n";

    WriteStringArrayJson(out, "pre_commands", opt.pre_commands, true);
    WriteStringArrayJson(out, "post_commands", opt.post_commands, false);

    out << "}\n";
}

// -----------------------------------------------------------------------------
// Usage / parsing helpers
// -----------------------------------------------------------------------------
static void PrintUsage(const char* prog)
{
    G4cout
        << "Usage: " << prog << " [options]\n\n"
        << "Options:\n"
        << "  --preset NAME                Apply a named preset first; later CLI args override it\n"
        << "  --beam-on N                  Number of events to run (default: 10)\n"
        << "  --generation-mode MODE       native | explicit (default: native)\n"
        << "  --qe on|off                  Enable quantum entanglement metadata/physics switch (default: on)\n"
	    << "  --three-gamma-model MODEL   approximate | ore-powell | ore-powell-polarized\n"
	    << "                               Select explicit 3-gamma kinematic backend\n"
	    << "                               (default: approximate)\n"
        << "  --3gamma-onfly on|off        Enable Geant4 3-gamma annihilation on fly (native mode) (default: off)\n"
        << "  --at-rest-model NAME         Positron at-rest model:\n"
        << "                               Simple | Allison | OrePawell | OrePowellPolar\n"
        << "                               (default: OrePowellPolar)\n"
        << "  --world-material NAME        World material name (default: G4_AIR)\n"
        << "  --orto-ps-fraction F         Native mode: set ortho-positronium fraction for world material\n"
        << "                               Range: 0..1 (default: unset)\n"
        << "\n"
        << "  Explicit-provider controls:\n"
        << "  --f-direct F                 Direct annihilation fraction\n"
        << "  --f-pps F                    Para-Ps fraction\n"
        << "  --f-ops F                    Ortho-Ps fraction\n"
        << "                               Fractions must sum to 1.0\n"
        << "  --ortho-3g-fraction F        Fraction of o-Ps events that decay via explicit 3-gamma branch\n"
        << "                               Range: 0..1 (default: 1.0)\n"
        << "  --delay-mode MODE            exponential | fixed (default: exponential)\n"
        << "  --tau-pps-ns T               Para-Ps lifetime in ns (default: 0.125)\n"
        << "  --tau-ops-ns T               Ortho-Ps lifetime in ns (default: 3.0)\n"
        << "  --fixed-delay-ns T           Fixed delay in ns when --delay-mode fixed (default: 3.0)\n"
        << "  --positron-range on|off      Enable explicit positron-range displacement (default: off)\n"
        << "  --positron-range-sigma-mm S  Positron-range Gaussian sigma in mm (default: 1.0)\n"
        << "\n"
        << "  Shared source controls:\n"
        << "  --prompt on|off              Enable prompt gamma source term (default: on)\n"
        << "  --prompt-energy-mev E        Prompt gamma energy in MeV (default: 1.274)\n"
        << "  --positron-kev E             Positron kinetic energy in keV (default: 0.0001)\n"
        << "  --source-mm X Y Z            Source position in mm (default: 0 0 0)\n"
        << "  --base-time-ns T             External base time in ns\n"
        << "  --use-external-base-time on|off\n"
        << "                               Enable external base time scheduling (default: off)\n"
        << "  --print-em-parameters        Print EM parameters after initialization\n"
        << "  --macro FILE                 Execute Geant4 macro after initialization\n"
        << "  --pre-cmd \"CMD\"             Apply Geant4 UI command before initialize (repeatable)\n"
        << "  --post-cmd \"CMD\"            Apply Geant4 UI command after initialize (repeatable)\n"
        << "  --no-run                     Initialize only; do not beamOn automatically\n"
        << "  --help                       Show this message\n\n"
        << "Presets:\n"
        << "  native_reference\n"
        << "  native_ortho_half\n"
        << "  explicit_3g_dev\n"
        << "  explicit_2g_reference\n"
        << "  explicit_mixed_qepet\n"
        << "  explicit_range_prompt\n\n"
        << "Examples:\n"
        << "  " << prog << " --preset native_reference --beam-on 100\n"
        << "  " << prog << " --preset native_ortho_half --beam-on 5000\n"
        << "  " << prog << " --preset explicit_3g_dev --beam-on 1000\n"
        << "  " << prog << " --preset explicit_3g_dev --prompt on --beam-on 100\n"
        << "  " << prog << " --generation-mode explicit --f-direct 0.3 --f-pps 0.2 --f-ops 0.5 --ortho-3g-fraction 1.0\n"
        << G4endl;
}

static bool ParseBool(const std::string& text)
{
    if (text == "1" || text == "true" || text == "TRUE" || text == "on" || text == "ON") return true;
    if (text == "0" || text == "false" || text == "FALSE" || text == "off" || text == "OFF") return false;
    throw std::runtime_error("Invalid boolean value: " + text);
}

static int ParseInt(const std::string& text, const std::string& label)
{
    try {
        size_t idx = 0;
        int value = std::stoi(text, &idx);
        if (idx != text.size()) throw std::runtime_error("");
        return value;
    } catch (...) {
        throw std::runtime_error("Invalid integer for " + label + ": " + text);
    }
}

static double ParseDouble(const std::string& text, const std::string& label)
{
    try {
        size_t idx = 0;
        double value = std::stod(text, &idx);
        if (idx != text.size()) throw std::runtime_error("");
        return value;
    } catch (...) {
        throw std::runtime_error("Invalid floating-point value for " + label + ": " + text);
    }
}

static GenerationModeChoice ParseGenerationMode(const std::string& text)
{
    if (text == "native" || text == "Native") return GenerationModeChoice::Native;
    if (text == "explicit" || text == "Explicit") return GenerationModeChoice::Explicit;
    throw std::runtime_error("Invalid --generation-mode: " + text + ". Allowed: native, explicit");
}

static DelayModeChoice ParseDelayMode(const std::string& text)
{
    if (text == "exponential" || text == "Exponential") return DelayModeChoice::Exponential;
    if (text == "fixed" || text == "Fixed") return DelayModeChoice::Fixed;
    throw std::runtime_error("Invalid --delay-mode: " + text + ". Allowed: exponential, fixed");
}

static void ValidatePositronAtRestModel(const std::string& model)
{
    if (model == "Simple") return;
    if (model == "Allison") return;
    if (model == "OrePawell") return;
    if (model == "OrePowellPolar") return;

    throw std::runtime_error(
        "Invalid --at-rest-model: " + model +
        ". Allowed: Simple, Allison, OrePawell, OrePowellPolar");
}

static void ValidateFractionTriplet(double f_direct, double f_pps, double f_ops)
{
    constexpr double kTol = 1e-9;
    if (f_direct < 0.0 || f_pps < 0.0 || f_ops < 0.0) {
        throw std::runtime_error("f_direct, f_pps, and f_ops must be non-negative.");
    }
    const double sum = f_direct + f_pps + f_ops;
    if (std::abs(sum - 1.0) > kTol) {
        throw std::runtime_error("f_direct + f_pps + f_ops must sum to 1.0.");
    }
}

static AppOptions ParseCommandLine(int argc, char** argv, AppOptions opt = AppOptions{})
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            std::exit(0);
        }
        else if (arg == "--preset") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --preset");
            opt.preset_name = argv[++i];
        }
        else if (arg == "--beam-on") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --beam-on");
            opt.beam_on = ParseInt(argv[++i], "--beam-on");
            opt.beam_on_specified = true;
            if (opt.beam_on < 0) throw std::runtime_error("--beam-on must be >= 0");
        }
        else if (arg == "--generation-mode") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --generation-mode");
            opt.generation_mode = ParseGenerationMode(argv[++i]);
        }
        else if (arg == "--qe") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --qe");
            opt.enable_qe = ParseBool(argv[++i]);
        }
	else if (arg == "--three-gamma-model") {
	    if (i + 1 >= argc) {
		throw std::runtime_error(
		    "Missing value for --three-gamma-model"
		);
	    }

	    opt.three_gamma_model =
		ParseThreeGammaModel(argv[++i]);
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
            if (opt.orto_ps_fraction < 0.0 || opt.orto_ps_fraction > 1.0) {
                throw std::runtime_error("--orto-ps-fraction must be in [0,1]");
            }
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
            if (opt.ortho_3g_fraction < 0.0 || opt.ortho_3g_fraction > 1.0) {
                throw std::runtime_error("--ortho-3g-fraction must be in [0,1]");
            }
        }
        else if (arg == "--delay-mode") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --delay-mode");
            opt.delay_mode = ParseDelayMode(argv[++i]);
        }
        else if (arg == "--tau-pps-ns") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --tau-pps-ns");
            opt.tau_pps_ns = ParseDouble(argv[++i], "--tau-pps-ns");
            if (opt.tau_pps_ns <= 0.0) throw std::runtime_error("--tau-pps-ns must be > 0");
        }
        else if (arg == "--tau-ops-ns") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --tau-ops-ns");
            opt.tau_ops_ns = ParseDouble(argv[++i], "--tau-ops-ns");
            if (opt.tau_ops_ns <= 0.0) throw std::runtime_error("--tau-ops-ns must be > 0");
        }
        else if (arg == "--fixed-delay-ns") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --fixed-delay-ns");
            opt.fixed_delay_ns = ParseDouble(argv[++i], "--fixed-delay-ns");
            if (opt.fixed_delay_ns < 0.0) throw std::runtime_error("--fixed-delay-ns must be >= 0");
        }
        else if (arg == "--positron-range") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --positron-range");
            opt.enable_positron_range = ParseBool(argv[++i]);
        }
        else if (arg == "--positron-range-sigma-mm") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --positron-range-sigma-mm");
            opt.positron_range_sigma_mm = ParseDouble(argv[++i], "--positron-range-sigma-mm");
            if (opt.positron_range_sigma_mm < 0.0) {
                throw std::runtime_error("--positron-range-sigma-mm must be >= 0");
            }
        }
        else if (arg == "--prompt") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --prompt");
            opt.has_prompt_gamma = ParseBool(argv[++i]);
        }
        else if (arg == "--prompt-energy-mev") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --prompt-energy-mev");
            opt.prompt_energy_mev = ParseDouble(argv[++i], "--prompt-energy-mev");
            if (opt.prompt_energy_mev <= 0.0) {
                throw std::runtime_error("--prompt-energy-mev must be > 0");
            }
        }
        else if (arg == "--positron-kev") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --positron-kev");
            opt.positron_kinetic_kev = ParseDouble(argv[++i], "--positron-kev");
            if (opt.positron_kinetic_kev <= 0.0) {
                throw std::runtime_error("--positron-kev must be > 0");
            }
        }
        else if (arg == "--source-mm") {
            if (i + 3 >= argc) throw std::runtime_error("Missing values for --source-mm X Y Z");
            opt.source_mm[0] = ParseDouble(argv[++i], "--source-mm X");
            opt.source_mm[1] = ParseDouble(argv[++i], "--source-mm Y");
            opt.source_mm[2] = ParseDouble(argv[++i], "--source-mm Z");
        }
        else if (arg == "--base-time-ns") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --base-time-ns");
            opt.base_time_ns = ParseDouble(argv[++i], "--base-time-ns");
        }
        else if (arg == "--use-external-base-time") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --use-external-base-time");
            opt.use_external_base_time = ParseBool(argv[++i]);
        }
        else if (arg == "--print-em-parameters") {
            opt.print_em_parameters = true;
        }
        else if (arg == "--macro") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --macro");
            opt.macro_file = argv[++i];
        }
        else if (arg == "--pre-cmd") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --pre-cmd");
            opt.pre_commands.emplace_back(argv[++i]);
        }
        else if (arg == "--post-cmd") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for --post-cmd");
            opt.post_commands.emplace_back(argv[++i]);
        }
        else if (arg == "--no-run") {
            opt.no_run = true;
        }
        else {
            throw std::runtime_error("Unknown option: " + arg);
        }
    }

    ValidateFractionTriplet(opt.f_direct, opt.f_pps, opt.f_ops);
    return opt;
}

// -----------------------------------------------------------------------------
// Helper: apply UI commands
// -----------------------------------------------------------------------------
static void ApplyRequiredCommand(const G4String& cmd)
{
    auto* ui = G4UImanager::GetUIpointer();
    if (!ui) {
        G4Exception("ApplyRequiredCommand",
                    "Positronium_200",
                    FatalException,
                    "G4UImanager::GetUIpointer() returned null.");
    }

    const G4int rc = ui->ApplyCommand(cmd);
    if (rc != 0) {
        G4ExceptionDescription msg;
        msg << "Failed UI command:\n  " << cmd << "\nReturn code = " << rc;
        G4Exception("ApplyRequiredCommand",
                    "Positronium_201",
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
        G4cout << "[main] Applied optional command: " << cmd << G4endl;
        return true;
    }

    G4cout << "[main] Optional command rejected (rc=" << rc << "): " << cmd << G4endl;
    return false;
}

// -----------------------------------------------------------------------------
// Minimal world-only detector
// -----------------------------------------------------------------------------
class SimpleDetector : public G4VUserDetectorConstruction {
public:
    explicit SimpleDetector(const std::string& world_material_name)
        : m_world_material_name(world_material_name)
    {
    }

    G4VPhysicalVolume* Construct() override
    {
        auto* nist = G4NistManager::Instance();
        auto* world_mat = nist->FindOrBuildMaterial(m_world_material_name, true);
        if (!world_mat) {
            G4ExceptionDescription msg;
            msg << "Failed to build/find world material: " << m_world_material_name;
            G4Exception("SimpleDetector::Construct",
                        "Positronium_202",
                        FatalException,
                        msg);
        }

        const G4double world_half = 1.0 * m;

        auto* solid_world = new G4Box("World", world_half, world_half, world_half);
        auto* logic_world = new G4LogicalVolume(solid_world, world_mat, "World");

        auto* phys_world = new G4PVPlacement(
            nullptr,
            G4ThreeVector(),
            logic_world,
            "World",
            nullptr,
            false,
            0,
            true
        );

        return phys_world;
    }

private:
    std::string m_world_material_name;
};

// -----------------------------------------------------------------------------
// Action initialization
// -----------------------------------------------------------------------------
class ActionInitialization : public G4VUserActionInitialization {
public:
    explicit ActionInitialization(const AppOptions& opt)
        : m_opt(opt)
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

            switch (m_opt.three_gamma_model) {
                case ThreeGammaModelChoice::Approximate:
                gen->SetThreeGammaModel(
                    PositroniumProvider::ThreeGammaModel::
                    ApproximatePhaseSpace
                );
                break;

                case ThreeGammaModelChoice::OrePowell:
                gen->SetThreeGammaModel(
                    PositroniumProvider::ThreeGammaModel::
                    Geant4OrePowell
                );
                break;

                case ThreeGammaModelChoice::OrePowellPolarized:
                gen->SetThreeGammaModel(
                    PositroniumProvider::ThreeGammaModel::
                    Geant4PolarizedOrePowell
                );
                break;
            }

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

        SetUserAction(gen);
    }

private:
    AppOptions m_opt;
};

// -----------------------------------------------------------------------------
// Direct EM configuration in C++
// -----------------------------------------------------------------------------
static void ConfigureEmParameters(const AppOptions& opt)
{
    auto* em = G4EmParameters::Instance();
    if (!em) {
        G4Exception("ConfigureEmParameters",
                    "Positronium_203",
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
        msg << "Failed to build/find material for positronium setup: "
            << opt.world_material;
        G4Exception("ConfigureMaterialPositronium",
                    "Positronium_204",
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
        G4cerr << "Argument error: " << e.what() << G4endl << G4endl;
        PrintUsage(argv[0]);
        return 1;
    }

    auto* run_manager = G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial);

    run_manager->SetUserInitialization(new SimpleDetector(opt.world_material));

    G4PhysListFactory phys_factory;
    auto* physics = phys_factory.GetReferencePhysList("FTFP_BERT");
    if (!physics) {
        G4Exception("main",
                    "Positronium_205",
                    FatalException,
                    "Failed to create reference physics list FTFP_BERT.");
    }

    physics->ReplacePhysics(new G4EmLivermorePolarizedPhysics());
    run_manager->SetUserInitialization(physics);

    run_manager->SetUserInitialization(new ActionInitialization(opt));

    ConfigureEmParameters(opt);
    ConfigureMaterialPositronium(opt);

    for (const auto& cmd : opt.pre_commands) {
        ApplyOptionalCommand(cmd);
    }

    run_manager->Initialize();

    G4cout << "\n=== Positronium Geant4 driver ===\n"
           << "Preset                     : " << (opt.preset_name.empty() ? "<none>" : opt.preset_name) << "\n"
           << "Generation mode            : " << GenerationModeName(opt.generation_mode) << "\n"
           << "Physics list               : FTFP_BERT + G4EmLivermorePolarizedPhysics\n"
           << "Quantum entanglement       : " << (opt.enable_qe ? "ON" : "OFF") << "\n"
           << "3-gamma model              : " << ThreeGammaModelName(opt.three_gamma_model) << "\n"
           << "3 gamma on fly             : " << (opt.enable_3gamma_onfly ? "ON" : "OFF") << "\n"
           << "At-rest model              : " << opt.positron_at_rest_model << "\n"
           << "World material             : " << opt.world_material << "\n"
           << "Native ortho-Ps fraction   : "
           << (opt.set_orto_ps_fraction ? std::to_string(opt.orto_ps_fraction) : std::string("<unset>"))
           << "\n"
           << "Explicit fractions         : direct=" << opt.f_direct
           << "  pPs=" << opt.f_pps
           << "  oPs=" << opt.f_ops << "\n"
           << "Explicit ortho 3g fraction : " << opt.ortho_3g_fraction << "\n"
           << "Delay mode                 : " << DelayModeName(opt.delay_mode) << "\n"
           << "Tau pPs (ns)               : " << opt.tau_pps_ns << "\n"
           << "Tau oPs (ns)               : " << opt.tau_ops_ns << "\n"
           << "Fixed delay (ns)           : " << opt.fixed_delay_ns << "\n"
           << "Positron range enabled     : " << (opt.enable_positron_range ? "ON" : "OFF") << "\n"
           << "Positron range sigma (mm)  : " << opt.positron_range_sigma_mm << "\n"
           << "Prompt gamma               : " << (opt.has_prompt_gamma ? "ON" : "OFF") << "\n"
           << "Prompt energy (MeV)        : " << opt.prompt_energy_mev << "\n"
           << "Positron KE (keV)          : " << opt.positron_kinetic_kev << "\n"
           << "Source position (mm)       : "
           << opt.source_mm[0] << " "
           << opt.source_mm[1] << " "
           << opt.source_mm[2] << "\n"
           << "Use external base time     : " << (opt.use_external_base_time ? "ON" : "OFF") << "\n"
           << "Base time (ns)             : " << opt.base_time_ns << "\n"
           << "Macro file                 : " << (opt.macro_file.empty() ? "<none>" : opt.macro_file) << "\n"
           << "Run config JSON            : run_config.json\n"
           << "BeamOn                     : " << opt.beam_on << "\n"
           << G4endl;

    if (opt.print_em_parameters) {
        ApplyRequiredCommand("/process/em/printParameters");
    }

    for (const auto& cmd : opt.post_commands) {
        ApplyOptionalCommand(cmd);
    }

    if (!opt.macro_file.empty()) {
        ApplyRequiredCommand(std::string("/control/execute ") + opt.macro_file);
    }

    if (!opt.no_run) {
        if (opt.macro_file.empty()) {
            if (opt.beam_on > 0) {
                ApplyRequiredCommand(std::string("/run/beamOn ") + std::to_string(opt.beam_on));
            }
        } else {
            if (opt.beam_on_specified && opt.beam_on > 0) {
                ApplyRequiredCommand(std::string("/run/beamOn ") + std::to_string(opt.beam_on));
            }
        }
    }

    delete run_manager;
    return 0;
}
