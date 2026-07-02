#include "G4DynamicParticle.hh"
#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4OrePowellAtRestModel.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "Randomize.hh"

#include <cerrno>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
    long events = 100000;
    long seed1 = 271828;
    long seed2 = 314159;

    std::string output =
        "annihilation_gammas.csv";

    std::string material_name =
        "G4_AIR";
};

long ParsePositiveLong(
    const std::string& text,
    const std::string& option_name
)
{
    errno = 0;

    char* end = nullptr;

    const long value =
        std::strtol(
            text.c_str(),
            &end,
            10
        );

    if (
        errno != 0 ||
        end == text.c_str() ||
        *end != '\0' ||
        value <= 0
    ) {
        throw std::invalid_argument(
            option_name +
            " must be a positive integer: " +
            text
        );
    }

    return value;
}

void PrintUsage(
    const char* program_name
)
{
    std::cout
        << "Usage:\n"
        << "  " << program_name
        << " [options]\n\n"
        << "Options:\n"
        << "  --events N\n"
        << "      Number of Ore-Powell events.\n"
        << "      Default: 100000\n\n"
        << "  --seed1 N\n"
        << "      First Geant4 random seed.\n"
        << "      Default: 271828\n\n"
        << "  --seed2 N\n"
        << "      Second Geant4 random seed.\n"
        << "      Default: 314159\n\n"
        << "  --output PATH\n"
        << "      Output photon CSV.\n"
        << "      Default: annihilation_gammas.csv\n\n"
        << "  --material NAME\n"
        << "      Geant4 NIST material name.\n"
        << "      Default: G4_AIR\n\n"
        << "  --help\n";
}

Options ParseOptions(
    int argc,
    char** argv
)
{
    Options options;

    for (
        int index = 1;
        index < argc;
        ++index
    ) {
        const std::string argument =
            argv[index];

        auto require_value =
            [&](const std::string& option)
            -> std::string
        {
            if (index + 1 >= argc) {
                throw std::invalid_argument(
                    "Missing value after " +
                    option
                );
            }

            ++index;
            return argv[index];
        };

        if (argument == "--events") {
            options.events =
                ParsePositiveLong(
                    require_value(argument),
                    argument
                );
        } else if (argument == "--seed1") {
            options.seed1 =
                ParsePositiveLong(
                    require_value(argument),
                    argument
                );
        } else if (argument == "--seed2") {
            options.seed2 =
                ParsePositiveLong(
                    require_value(argument),
                    argument
                );
        } else if (argument == "--output") {
            options.output =
                require_value(argument);

            if (options.output.empty()) {
                throw std::invalid_argument(
                    "--output cannot be empty"
                );
            }
        } else if (argument == "--material") {
            options.material_name =
                require_value(argument);

            if (options.material_name.empty()) {
                throw std::invalid_argument(
                    "--material cannot be empty"
                );
            }
        } else if (
            argument == "--help" ||
            argument == "-h"
        ) {
            PrintUsage(argv[0]);
            std::exit(0);
        } else {
            throw std::invalid_argument(
                "Unknown option: " +
                argument
            );
        }
    }

    return options;
}

void DeleteParticles(
    std::vector<G4DynamicParticle*>& particles
)
{
    for (
        G4DynamicParticle* particle :
        particles
    ) {
        delete particle;
    }

    particles.clear();
}

}  // namespace

int main(
    int argc,
    char** argv
)
{
    try {
        const Options options =
            ParseOptions(argc, argv);

        long seeds[2] = {
            options.seed1,
            options.seed2
        };

        G4Random::setTheSeeds(seeds);

        G4Material* material =
            G4NistManager::Instance()
                ->FindOrBuildMaterial(
                    options.material_name,
                    true
                );

        if (!material) {
            throw std::runtime_error(
                "Could not build material: " +
                options.material_name
            );
        }

        G4OrePowellAtRestModel model;

        std::ofstream output(
            options.output,
            std::ios::out |
            std::ios::trunc
        );

        if (!output) {
            throw std::runtime_error(
                "Could not open output file: " +
                options.output
            );
        }

        /*
         * Match the PsSource photon CSV core schema.
         *
         * The synthetic track IDs 1, 2, and 3 provide a deterministic
         * photon labeling for the symmetric analytic validator.
         *
         * Polarization is not generated by the ordinary
         * G4OrePowellAtRestModel, so polarization fields are zero and
         * polarization_valid is false.
         */
        output
            << "event_id,"
            << "source_event_id,"
            << "track_id,"
            << "parent_id,"
            << "creator_process,"
            << "vertex_time_ns,"
            << "vertex_x_mm,"
            << "vertex_y_mm,"
            << "vertex_z_mm,"
            << "kinetic_energy_MeV,"
            << "dir_x,"
            << "dir_y,"
            << "dir_z,"
            << "pol_x,"
            << "pol_y,"
            << "pol_z,"
            << "polarization_valid\n";

        output
            << std::setprecision(
                std::numeric_limits<double>::max_digits10
            );

        long generated_photons = 0;
        double maximum_local_energy_deposit = 0.0;

        for (
            long event_id = 0;
            event_id < options.events;
            ++event_id
        ) {
            std::vector<G4DynamicParticle*>
                secondaries;

            G4double local_energy_deposit = 0.0;

            model.SampleSecondaries(
                secondaries,
                local_energy_deposit,
                material
            );

            if (secondaries.size() != 3) {
                const std::size_t count =
                    secondaries.size();

                DeleteParticles(secondaries);

                throw std::runtime_error(
                    "Event " +
                    std::to_string(event_id) +
                    " returned " +
                    std::to_string(count) +
                    " photons instead of 3"
                );
            }

            maximum_local_energy_deposit =
                std::max(
                    maximum_local_energy_deposit,
                    static_cast<double>(
                        local_energy_deposit / MeV
                    )
                );

            for (
                std::size_t photon_index = 0;
                photon_index < secondaries.size();
                ++photon_index
            ) {
                G4DynamicParticle* particle =
                    secondaries[photon_index];

                if (!particle) {
                    DeleteParticles(
                        secondaries
                    );

                    throw std::runtime_error(
                        "Null secondary in event " +
                        std::to_string(event_id)
                    );
                }

                const G4ThreeVector direction =
                    particle
                        ->GetMomentumDirection();

                output
                    << event_id << ","
                    << (event_id + 1) << ","
                    << (photon_index + 1) << ","
                    << 0 << ","
                    << "native_G4OrePowellAtRestModel"
                    << ","
                    << 0.0 << ","
                    << 0.0 << ","
                    << 0.0 << ","
                    << 0.0 << ","
                    << (
                        particle
                            ->GetKineticEnergy() /
                        MeV
                    )
                    << ","
                    << direction.x() << ","
                    << direction.y() << ","
                    << direction.z() << ","
                    << 0.0 << ","
                    << 0.0 << ","
                    << 0.0 << ","
                    << 0
                    << "\n";

                ++generated_photons;
            }

            DeleteParticles(secondaries);
        }

        output.close();

        if (!output) {
            throw std::runtime_error(
                "Error while writing output file: " +
                options.output
            );
        }

        std::cout
            << "=== Native Geant4 Ore-Powell reference ===\n"
            << "Events                 : "
            << options.events << "\n"
            << "Photons                : "
            << generated_photons << "\n"
            << "Seed 1                 : "
            << options.seed1 << "\n"
            << "Seed 2                 : "
            << options.seed2 << "\n"
            << "Material               : "
            << options.material_name << "\n"
            << "Maximum local deposit  : "
            << maximum_local_energy_deposit
            << " MeV\n"
            << "Output                  : "
            << options.output << "\n"
            << "PASS\n";

        return 0;
    } catch (
        const std::exception& error
    ) {
        std::cerr
            << "ERROR: "
            << error.what()
            << "\n";

        return 1;
    }
}
