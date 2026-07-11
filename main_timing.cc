#include "PositroniumGenerator.hh"
#include "PositroniumTruthInfo.hh"
#include "PsTerminalStateBuilder.hh"

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
#include "G4RunManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4SDManager.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4StepStatus.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4UImanager.hh"
#include "G4UserEventAction.hh"
#include "G4UserTrackingAction.hh"
#include "G4VProcess.hh"
#include "G4VSensitiveDetector.hh"
#include "G4VUserActionInitialization.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4ios.hh"
#include "G4Positron.hh"

#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Options
// -----------------------------------------------------------------------------
enum class GenerationModeChoice {
    Native,
    Explicit,
    TransportCoupled
};
enum class DelayModeChoice { Exponential, Fixed };
enum class ThreeGammaModelChoice {
    Approximate,
    OrePowell,
    OrePowellPolarized
};

struct AppOptions {
    int beam_on = 10000;
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

    // Explicit provider controls
    double f_direct = 0.3;
    double f_pps = 0.2;
    double f_ops = 0.5;

    ThreeGammaModelChoice three_gamma_model =
        ThreeGammaModelChoice::Approximate;
    double ortho_3g_fraction = 1.0;
    DelayModeChoice delay_mode = DelayModeChoice::Exponential;
    double tau_pps_ns = 0.125;
    double tau_ops_ns = 3.0;
    double fixed_delay_ns = 3.0;

    bool enable_positron_range = false;
    double positron_range_sigma_mm = 1.0;

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
static const char* GenerationModeName(
    GenerationModeChoice mode
)
{
    switch (mode) {
        case GenerationModeChoice::Native:
            return "native";

        case GenerationModeChoice::Explicit:
            return "explicit";

        case GenerationModeChoice::TransportCoupled:
            return "transport-coupled";
    }

    return "unknown";
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
        << "  --beam-on N                  Number of events to run (default: 10000)\n"
	<< "  --generation-mode MODE       native | explicit | transport-coupled\n"
	<< "                               (default: native)\n"
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
        << "  " << prog << " --generation-mode native --at-rest-model OrePowellPolar --orto-ps-fraction 0.5\n"
        << "  " << prog << " --generation-mode explicit --f-direct 0 --f-pps 0 --f-ops 1 --tau-ops-ns 3.0 --prompt off\n"
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
        const int value = std::stoi(text, &idx);
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
        const double value = std::stod(text, &idx);
        if (idx != text.size()) throw std::runtime_error("");
        return value;
    } catch (...) {
        throw std::runtime_error("Invalid floating-point value for " + label + ": " + text);
    }
}

static GenerationModeChoice ParseGenerationMode(
    const std::string& text
)
{
    if (text == "native" || text == "Native") {
        return GenerationModeChoice::Native;
    }

    if (text == "explicit" || text == "Explicit") {
        return GenerationModeChoice::Explicit;
    }

    if (
        text == "transport-coupled" ||
        text == "TransportCoupled"
    ) {
        return GenerationModeChoice::TransportCoupled;
    }

    throw std::runtime_error(
        "Invalid --generation-mode: " + text +
        ". Allowed: native, explicit, transport-coupled"
    );
}

static DelayModeChoice ParseDelayMode(const std::string& text)
{
    if (text == "exponential" || text == "Exponential") return DelayModeChoice::Exponential;
    if (text == "fixed" || text == "Fixed") return DelayModeChoice::Fixed;
    throw std::runtime_error("Invalid --delay-mode: " + text + ". Allowed: exponential, fixed");
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

    if (
        opt.generation_mode ==
            GenerationModeChoice::TransportCoupled &&
        opt.enable_positron_range
    ) {
        throw std::runtime_error(
            "--positron-range cannot be enabled in transport-coupled mode. "
            "Geant4 positron transport determines the terminal position."
        );
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
                    "Positronium_300",
                    FatalException,
                    "G4UImanager::GetUIpointer() returned null.");
    }

    const G4int rc = ui->ApplyCommand(cmd);
    if (rc != 0) {
        G4ExceptionDescription msg;
        msg << "Failed UI command:\n  " << cmd << "\nReturn code = " << rc;
        G4Exception("ApplyRequiredCommand",
                    "Positronium_301",
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
        G4cout << "[ps_timing] Applied optional command: " << cmd << G4endl;
        return true;
    }

    G4cout << "[ps_timing] Optional command rejected (rc=" << rc << "): " << cmd << G4endl;
    return false;
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

// -----------------------------------------------------------------------------
// Sensitive detector: records boundary crossings into the timing slabs
// -----------------------------------------------------------------------------
class TimeHitSD : public G4VSensitiveDetector {
public:
    TimeHitSD(const G4String& name,
              const std::string& filename,
              const std::string& det_name)
        : G4VSensitiveDetector(name),
          m_filename(filename),
          m_det_name(det_name)
    {
    }

    ~TimeHitSD() override
    {
        if (m_out.is_open()) {
            m_out.close();
        }
    }

    void Initialize(G4HCofThisEvent*) override
    {
        if (m_initialized_file) {
            return;
        }

        const bool exists = static_cast<bool>(std::ifstream(m_filename));
        m_out.open(m_filename, std::ios::out | std::ios::app);

        if (!m_out) {
            G4ExceptionDescription msg;
            msg << "Failed to open output CSV: " << m_filename;
            G4Exception("TimeHitSD::Initialize",
                        "Positronium_302",
                        FatalException,
                        msg);
        }

        if (!exists) {
            m_out << "event_id,track_id,parent_id,pdg_code,creator_process,time_ns,"
                     "kinetic_energy_MeV,x_mm,y_mm,z_mm,det_name\n";
        }

        m_initialized_file = true;
    }

    G4bool ProcessHits(G4Step* step, G4TouchableHistory*) override
    {
        auto* pre = step->GetPreStepPoint();
        auto* track = step->GetTrack();

        if (pre->GetStepStatus() != fGeomBoundary) {
            return false;
        }

        auto* event = G4RunManager::GetRunManager()->GetCurrentEvent();
        const int event_id = event ? event->GetEventID() : -1;

        const int track_id = track->GetTrackID();
        const int parent_id = track->GetParentID();
        const int pdg = track->GetParticleDefinition()->GetPDGEncoding();
        const std::string creator_name = SafeProcessName(track->GetCreatorProcess());

        const double time_ns = pre->GetGlobalTime() / ns;
        const double kinetic_energy_mev = pre->GetKineticEnergy() / MeV;
        const auto pos = pre->GetPosition();

        m_out << event_id << ","
              << track_id << ","
              << parent_id << ","
              << pdg << ","
              << creator_name << ","
              << std::fixed << std::setprecision(6)
              << time_ns << ","
              << kinetic_energy_mev << ","
              << pos.x() / mm << ","
              << pos.y() / mm << ","
              << pos.z() / mm << ","
              << m_det_name
              << "\n";

        return true;
    }

private:
    std::string m_filename;
    std::string m_det_name;
    std::ofstream m_out;
    bool m_initialized_file = false;
};

// -----------------------------------------------------------------------------
// Event-level annihilation truth capture
// -----------------------------------------------------------------------------
class AnnihilationTruthEventAction : public G4UserEventAction {
public:
    struct GammaBirthRecord {
        int track_id = -1;
        int parent_id = -1;
        std::string creator_process = "UNKNOWN";

        double vertex_time_ns = -1.0;
        double vertex_x_mm = 0.0;
        double vertex_y_mm = 0.0;
        double vertex_z_mm = 0.0;

        double kinetic_energy_mev = 0.0;
        double dir_x = 0.0;
        double dir_y = 0.0;
        double dir_z = 0.0;

	double pol_x = 0.0;
	double pol_y = 0.0;
	double pol_z = 0.0;
	bool polarization_valid = false;

    };

    std::string m_physics_model_name;
    std::string m_physics_model_version;
    std::string m_physics_validation_status;

    AnnihilationTruthEventAction()
    {
        m_summary_out.open("annihilation_summary.csv", std::ios::out | std::ios::trunc);
        if (!m_summary_out) {
            G4Exception("AnnihilationTruthEventAction",
                        "Positronium_303",
                        FatalException,
                        "Failed to open annihilation_summary.csv");
        }

	m_summary_out
	    << "event_id,source_event_id,qe_requested,has_prompt_gamma,"
	    << "source_x_mm,source_y_mm,source_z_mm,"
	    << "annihilation_found,annihilation_mode,n_annihilation_gammas,"
	    << "annihilation_time_ns,annihilation_x_mm,annihilation_y_mm,"
	    << "annihilation_z_mm,positron_range_mm,"
	    << "physics_model_name,physics_model_version,"
	    << "physics_validation_status\n";

        m_gamma_out.open("annihilation_gammas.csv", std::ios::out | std::ios::trunc);
        if (!m_gamma_out) {
            G4Exception("AnnihilationTruthEventAction",
                        "Positronium_304",
                        FatalException,
                        "Failed to open annihilation_gammas.csv");
        }

        m_gamma_out
            << "event_id,source_event_id,track_id,parent_id,creator_process,"
            << "vertex_time_ns,vertex_x_mm,vertex_y_mm,vertex_z_mm,"
            << "kinetic_energy_MeV,dir_x,dir_y,dir_z,"
            << "pol_x,pol_y,pol_z,polarization_valid\n";
    }

    ~AnnihilationTruthEventAction() override
    {
        if (m_summary_out.is_open()) m_summary_out.close();
        if (m_gamma_out.is_open()) m_gamma_out.close();
    }

    void BeginOfEventAction(const G4Event* event) override
    {
        m_event_id = event ? event->GetEventID() : -1;

        m_source_event_id = 0;
        m_qe_requested = false;
        m_has_prompt_gamma = false;
        m_prompt_energy_mev = 0.0;
        m_source_x_mm = 0.0;
        m_source_y_mm = 0.0;
        m_source_z_mm = 0.0;

	m_physics_model_name = "unknown";
	m_physics_model_version = "unknown";
	m_physics_validation_status = "unknown";

        m_declared_annihilation_mode = -1;
        m_declared_delay_ns = -1.0;
        m_declared_ann_x_mm = std::numeric_limits<double>::quiet_NaN();
        m_declared_ann_y_mm = std::numeric_limits<double>::quiet_NaN();
        m_declared_ann_z_mm = std::numeric_limits<double>::quiet_NaN();
        m_explicit_truth_available = false;

        if (event) {
            auto* truth = dynamic_cast<PositroniumTruthInfo*>(event->GetUserInformation());
            if (truth) {
                m_source_event_id = truth->source_event_id;
                m_qe_requested = truth->qe_mode;
                m_has_prompt_gamma = truth->has_prompt_gamma;
                m_prompt_energy_mev = truth->prompt_energy_MeV;
                m_source_x_mm = truth->source_x_mm;
                m_source_y_mm = truth->source_y_mm;
                m_source_z_mm = truth->source_z_mm;

		m_physics_model_name =
		    truth->physics_model_name;

		m_physics_model_version =
		    truth->physics_model_version;

		m_physics_validation_status =
		    truth->physics_validation_status;

                m_declared_annihilation_mode = truth->annihilation_mode;
                m_declared_delay_ns = truth->delay_ns;
                m_declared_ann_x_mm = truth->ann_x_mm;
                m_declared_ann_y_mm = truth->ann_y_mm;
                m_declared_ann_z_mm = truth->ann_z_mm;

                m_explicit_truth_available =
                    (truth->annihilation_mode > 0) &&
                    std::isfinite(truth->delay_ns) &&
                    std::isfinite(truth->ann_x_mm) &&
                    std::isfinite(truth->ann_y_mm) &&
                    std::isfinite(truth->ann_z_mm);
            }
        }

        m_annihilation_found = false;
        m_annihilation_time_ns = -1.0;
        m_annihilation_x_mm = std::numeric_limits<double>::quiet_NaN();
        m_annihilation_y_mm = std::numeric_limits<double>::quiet_NaN();
        m_annihilation_z_mm = std::numeric_limits<double>::quiet_NaN();
        m_annihilation_gammas.clear();
    }

    void EndOfEventAction(const G4Event*) override
    {
        if (!m_annihilation_found && m_explicit_truth_available) {
            m_annihilation_found = true;
            m_annihilation_time_ns = m_declared_delay_ns;
            m_annihilation_x_mm = m_declared_ann_x_mm;
            m_annihilation_y_mm = m_declared_ann_y_mm;
            m_annihilation_z_mm = m_declared_ann_z_mm;
        }

        double positron_range_mm = -1.0;
        if (m_annihilation_found) {
            const double dx = m_annihilation_x_mm - m_source_x_mm;
            const double dy = m_annihilation_y_mm - m_source_y_mm;
            const double dz = m_annihilation_z_mm - m_source_z_mm;
            positron_range_mm = std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        const int reported_mode =
            (m_annihilation_found && !m_annihilation_gammas.empty())
                ? static_cast<int>(m_annihilation_gammas.size())
                : (m_explicit_truth_available ? m_declared_annihilation_mode : 0);

        const int reported_ngam =
            (m_annihilation_found && !m_annihilation_gammas.empty())
                ? static_cast<int>(m_annihilation_gammas.size())
                : (m_explicit_truth_available ? m_declared_annihilation_mode : 0);

        m_summary_out
            << m_event_id << ","
            << m_source_event_id << ","
            << (m_qe_requested ? 1 : 0) << ","
            << (m_has_prompt_gamma ? 1 : 0) << ","
            << std::fixed << std::setprecision(6)
            << m_source_x_mm << ","
            << m_source_y_mm << ","
            << m_source_z_mm << ","
            << (m_annihilation_found ? 1 : 0) << ","
            << reported_mode << ","
            << reported_ngam << ","
            << m_annihilation_time_ns << ","
            << m_annihilation_x_mm << ","
            << m_annihilation_y_mm << ","
            << m_annihilation_z_mm << ","
            << positron_range_mm << ","
            << m_physics_model_name << ","
            << m_physics_model_version << ","
            << m_physics_validation_status
            << "\n";

        for (const auto& gamma_rec : m_annihilation_gammas) {
            m_gamma_out
                << m_event_id << ","
                << m_source_event_id << ","
                << gamma_rec.track_id << ","
                << gamma_rec.parent_id << ","
                << gamma_rec.creator_process << ","
                << std::fixed << std::setprecision(12)
                << gamma_rec.vertex_time_ns << ","
                << gamma_rec.vertex_x_mm << ","
                << gamma_rec.vertex_y_mm << ","
                << gamma_rec.vertex_z_mm << ","
                << gamma_rec.kinetic_energy_mev << ","
		<< gamma_rec.dir_x << ","
		<< gamma_rec.dir_y << ","
		<< gamma_rec.dir_z << ","
		<< gamma_rec.pol_x << ","
		<< gamma_rec.pol_y << ","
		<< gamma_rec.pol_z << ","
		<< (gamma_rec.polarization_valid ? 1 : 0)
		<< "\n";
        }
    }

    void RecordAnnihilationGamma(const G4Track* track)
    {
        GammaBirthRecord rec;
        rec.track_id = track->GetTrackID();
        rec.parent_id = track->GetParentID();
        rec.creator_process = SafeProcessName(track->GetCreatorProcess());

        const auto& vtx = track->GetVertexPosition();
        rec.vertex_time_ns = track->GetGlobalTime() / ns;
        rec.vertex_x_mm = vtx.x() / mm;
        rec.vertex_y_mm = vtx.y() / mm;
        rec.vertex_z_mm = vtx.z() / mm;

        rec.kinetic_energy_mev = track->GetKineticEnergy() / MeV;

        const auto& dir = track->GetMomentumDirection();
        rec.dir_x = dir.x();
        rec.dir_y = dir.y();
        rec.dir_z = dir.z();

	const auto& polarization =
	    track->GetPolarization();

	rec.pol_x = polarization.x();
	rec.pol_y = polarization.y();
	rec.pol_z = polarization.z();

	const double polarization_magnitude_squared =
	    polarization.mag2();

	rec.polarization_valid =
	    polarization_magnitude_squared > 0.0;

        if (!m_annihilation_found) {
            m_annihilation_found = true;
            m_annihilation_time_ns = rec.vertex_time_ns;
            m_annihilation_x_mm = rec.vertex_x_mm;
            m_annihilation_y_mm = rec.vertex_y_mm;
            m_annihilation_z_mm = rec.vertex_z_mm;
        }

        m_annihilation_gammas.push_back(rec);
    }

    bool IsExpectedExplicitPrimaryGamma(const G4Track* track) const
    {
        if (!m_explicit_truth_available) return false;
        if (!track) return false;
        if (track->GetParticleDefinition()->GetPDGEncoding() != 22) return false;
        if (track->GetParentID() != 0) return false;
        if (track->GetCreatorProcess() != nullptr) return false;

        const double t_ns = track->GetGlobalTime() / ns;
        const auto& vtx = track->GetVertexPosition();
        const double x_mm = vtx.x() / mm;
        const double y_mm = vtx.y() / mm;
        const double z_mm = vtx.z() / mm;
        const double e_mev = track->GetKineticEnergy() / MeV;

        constexpr double kTimeTolNs = 1.0e-6;
        constexpr double kPosTolMm  = 1.0e-6;
        constexpr double kEnergyTolMeV = 1.0e-6;

        // Explicit prompt gamma: reject it
        if (m_has_prompt_gamma) {
            const bool matches_source =
                (std::abs(t_ns - 0.0) <= kTimeTolNs) &&
                (std::abs(x_mm - m_source_x_mm) <= kPosTolMm) &&
                (std::abs(y_mm - m_source_y_mm) <= kPosTolMm) &&
                (std::abs(z_mm - m_source_z_mm) <= kPosTolMm);
            const bool matches_prompt_energy =
                (std::abs(e_mev - m_prompt_energy_mev) <= kEnergyTolMeV);

            if (matches_source && matches_prompt_energy) {
                return false;
            }
        }

        return (std::abs(t_ns - m_declared_delay_ns) <= kTimeTolNs) &&
               (std::abs(x_mm - m_declared_ann_x_mm) <= kPosTolMm) &&
               (std::abs(y_mm - m_declared_ann_y_mm) <= kPosTolMm) &&
               (std::abs(z_mm - m_declared_ann_z_mm) <= kPosTolMm);
    }

    std::uint64_t GetSourceEventId() const
    {
        return m_source_event_id;
    }

private:
    int m_event_id = -1;

    uint64_t m_source_event_id = 0;
    bool m_qe_requested = false;
    bool m_has_prompt_gamma = false;
    double m_prompt_energy_mev = 0.0;
    double m_source_x_mm = 0.0;
    double m_source_y_mm = 0.0;
    double m_source_z_mm = 0.0;

    int m_declared_annihilation_mode = -1;
    double m_declared_delay_ns = -1.0;
    double m_declared_ann_x_mm = std::numeric_limits<double>::quiet_NaN();
    double m_declared_ann_y_mm = std::numeric_limits<double>::quiet_NaN();
    double m_declared_ann_z_mm = std::numeric_limits<double>::quiet_NaN();
    bool m_explicit_truth_available = false;

    bool m_annihilation_found = false;
    double m_annihilation_time_ns = -1.0;
    double m_annihilation_x_mm = std::numeric_limits<double>::quiet_NaN();
    double m_annihilation_y_mm = std::numeric_limits<double>::quiet_NaN();
    double m_annihilation_z_mm = std::numeric_limits<double>::quiet_NaN();

    std::vector<GammaBirthRecord> m_annihilation_gammas;

    std::ofstream m_summary_out;
    std::ofstream m_gamma_out;
};

// -----------------------------------------------------------------------------
// Tracking action: intercept annihilation gamma birth
// -----------------------------------------------------------------------------
class AnnihilationTruthTrackingAction : public G4UserTrackingAction {
public:
    explicit AnnihilationTruthTrackingAction(AnnihilationTruthEventAction* event_action)
        : m_event_action(event_action)
    {
    }

    void PreUserTrackingAction(const G4Track* track) override
    {
        if (!m_event_action) return;

        if (IsNativeAnnihilationGammaTrack(track) ||
            m_event_action->IsExpectedExplicitPrimaryGamma(track)) {
            m_event_action->RecordAnnihilationGamma(track);
        }
    }

    void PostUserTrackingAction(const G4Track* track) override
    {
        if (!m_event_action || !track) {
            return;
        }

        if (
            track->GetParticleDefinition() !=
            G4Positron::PositronDefinition()
        ) {
            return;
        }

        const G4Step* final_step = track->GetStep();
        if (!final_step) {
            return;
        }

        const G4StepPoint* post_step =
            final_step->GetPostStepPoint();

        if (!post_step) {
            return;
        }

        const G4VProcess* terminating_process =
            post_step->GetProcessDefinedStep();

        const std::string process_name =
            SafeProcessName(terminating_process);

        if (process_name != "annihil") {
            return;
        }

        const PsTerminalState state =
            PsTerminalStateBuilder::Build(
                *track,
                *final_step,
                m_event_action->GetSourceEventId()
            );

        G4cout
            << "[PsTerminalState]"
            << " source_event_id=" << state.source_event_id
            << " track_id=" << state.positron_track_id
            << " process=" << process_name
            << " terminal_time_ns=" << state.terminal_time_ns
            << " terminal_position_mm=("
            << state.terminal_position.x() / mm << ","
            << state.terminal_position.y() / mm << ","
            << state.terminal_position.z() / mm << ")"
            << " track_length_mm=" << state.track_length_mm
            << " displacement_mm="
            << state.source_to_terminal_distance_mm
            << " terminal_ke_MeV="
            << state.terminal_kinetic_energy_MeV
            << " material="
            << (
                state.material
                    ? state.material->GetName()
                    : "NULL"
            )
            << " physical_volume="
            << (
                state.physical_volume
                    ? state.physical_volume->GetName()
                    : "NULL"
            )
            << " logical_volume="
            << (
                state.logical_volume
                    ? state.logical_volume->GetName()
                    : "NULL"
            )
            << " region="
            << (
                state.region
                    ? state.region->GetName()
                    : "NULL"
            )
            << " copy_number="
            << state.copy_number
            << " touchable_depth="
            << state.touchable_copy_numbers.size()
            << G4endl;

    }

private:
    AnnihilationTruthEventAction* m_event_action = nullptr;
};

// -----------------------------------------------------------------------------
// Detector: world + two thin timing slabs at +/- Z
// -----------------------------------------------------------------------------
class TimingDetectorConstruction : public G4VUserDetectorConstruction {
public:
    explicit TimingDetectorConstruction(const std::string& world_material_name)
        : m_world_material_name(world_material_name)
    {
    }

    ~TimingDetectorConstruction() override = default;

    G4VPhysicalVolume* Construct() override
    {
        auto* nist = G4NistManager::Instance();

        auto* world_mat = nist->FindOrBuildMaterial(m_world_material_name, true);
        if (!world_mat) {
            G4ExceptionDescription msg;
            msg << "Failed to build/find world material: " << m_world_material_name;
            G4Exception("TimingDetectorConstruction::Construct",
                        "Positronium_307",
                        FatalException,
                        msg);
        }

        auto* det_mat = nist->FindOrBuildMaterial("G4_Si");

        const G4double world_half = 500.0 * mm;

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

        auto* solid_det_plus = new G4Box("TimingDetPlus", 50.0 * mm, 50.0 * mm, 0.5 * mm);
        m_logic_det_plus = new G4LogicalVolume(solid_det_plus, det_mat, "TimingDetPlus");
        new G4PVPlacement(
            nullptr,
            G4ThreeVector(0.0, 0.0, +100.0 * mm),
            m_logic_det_plus,
            "TimingDetPlus",
            logic_world,
            false,
            0,
            true
        );

        auto* solid_det_minus = new G4Box("TimingDetMinus", 50.0 * mm, 50.0 * mm, 0.5 * mm);
        m_logic_det_minus = new G4LogicalVolume(solid_det_minus, det_mat, "TimingDetMinus");
        new G4PVPlacement(
            nullptr,
            G4ThreeVector(0.0, 0.0, -100.0 * mm),
            m_logic_det_minus,
            "TimingDetMinus",
            logic_world,
            false,
            0,
            true
        );

        return phys_world;
    }

    void ConstructSDandField() override
    {
        auto* sdman = G4SDManager::GetSDMpointer();

        auto* sd_plus = new TimeHitSD("TimingSDPlus", "hits_plus.csv", "plus");
        auto* sd_minus = new TimeHitSD("TimingSDMinus", "hits_minus.csv", "minus");

        sdman->AddNewDetector(sd_plus);
        sdman->AddNewDetector(sd_minus);

        SetSensitiveDetector(m_logic_det_plus, sd_plus);
        SetSensitiveDetector(m_logic_det_minus, sd_minus);
    }

private:
    std::string m_world_material_name;
    G4LogicalVolume* m_logic_det_plus = nullptr;
    G4LogicalVolume* m_logic_det_minus = nullptr;
};

// -----------------------------------------------------------------------------
// Action initialization
// -----------------------------------------------------------------------------
class TimingActionInitialization : public G4VUserActionInitialization {
public:
    explicit TimingActionInitialization(const AppOptions& opt)
        : m_opt(opt)
    {
    }

    void Build() const override
    {
        auto* gen = new PositroniumGenerator();

	switch (m_opt.generation_mode) {
	    case GenerationModeChoice::Native:
		gen->SetGenerationMode(
		    PositroniumGenerator::GenerationMode::NativeGeant4
		);
		break;

	    case GenerationModeChoice::Explicit:
		gen->SetGenerationMode(
		    PositroniumGenerator::GenerationMode::ExplicitProvider
		);
		break;

	    case GenerationModeChoice::TransportCoupled:
		gen->SetGenerationMode(
		    PositroniumGenerator::GenerationMode::TransportCoupled
		);
		break;
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

        auto* event_action = new AnnihilationTruthEventAction();
        auto* tracking_action = new AnnihilationTruthTrackingAction(event_action);

        SetUserAction(gen);
        SetUserAction(event_action);
        SetUserAction(tracking_action);
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
                    "Positronium_306",
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
                    "Positronium_308",
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

    run_manager->SetUserInitialization(new TimingDetectorConstruction(opt.world_material));

    G4PhysListFactory phys_factory;
    auto* physics = phys_factory.GetReferencePhysList("FTFP_BERT");
    if (!physics) {
        G4Exception("main",
                    "Positronium_305",
                    FatalException,
                    "Failed to create reference physics list FTFP_BERT.");
    }

    physics->ReplacePhysics(new G4EmLivermorePolarizedPhysics());
    run_manager->SetUserInitialization(physics);

    run_manager->SetUserInitialization(new TimingActionInitialization(opt));

    ConfigureEmParameters(opt);
    ConfigureMaterialPositronium(opt);

    for (const auto& cmd : opt.pre_commands) {
        ApplyOptionalCommand(cmd);
    }

    run_manager->Initialize();

    G4cout << "\n=== Positronium Geant4 timing harness with truth capture ===\n"
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
           << "Outputs                    : hits_plus.csv, hits_minus.csv, annihilation_summary.csv, annihilation_gammas.csv\n"
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
