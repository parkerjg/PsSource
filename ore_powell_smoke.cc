#include "OrePowellPsModel.hh"

#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace {

double Norm(
    const std::array<double, 3>& vector
)
{
    return std::sqrt(
        vector[0] * vector[0] +
        vector[1] * vector[1] +
        vector[2] * vector[2]
    );
}

}  // namespace

int main(int argc, char** argv)
{
    int event_count = 10000;

    if (argc > 1) {
        event_count = std::stoi(argv[1]);
    }

    if (event_count <= 0) {
        throw std::invalid_argument(
            "Event count must be positive."
        );
    }

    PsEnvironment environment;
    environment.f_direct = 0.0;
    environment.f_pps = 0.0;
    environment.f_ops = 1.0;

    environment.tau_ops_ns = 3.0;
    environment.ops_2g_fraction = 0.0;
    environment.ops_3g_fraction = 1.0;

    OrePowellPsModel model(
        OrePowellPsModel::PolarizationMode::Polarized
    );

    double energy_error_sum = 0.0;
    double maximum_energy_error = 0.0;

    double momentum_error_sum = 0.0;
    double maximum_momentum_error = 0.0;

    double delay_sum = 0.0;

    std::array<double, 3> direction_sum = {
        0.0, 0.0, 0.0
    };

    std::array<double, 3> direction_square_sum = {
        0.0, 0.0, 0.0
    };

    int photon_count = 0;
    int polarized_photon_count = 0;

    const double expected_total_energy_MeV =
        2.0 * electron_mass_c2 / MeV;

    for (int event_index = 0;
         event_index < event_count;
         ++event_index) {

        const PsModelResult result =
            model.Sample(environment);

        if (result.annihilation_mode != 3) {
            throw std::runtime_error(
                "Ore-Powell model returned a non-3-gamma event."
            );
        }

        if (result.photons.size() != 3) {
            throw std::runtime_error(
                "Ore-Powell model did not return three photons."
            );
        }

        double energy_sum = 0.0;

        std::array<double, 3> momentum_sum = {
            0.0, 0.0, 0.0
        };

        for (const PsPhoton& photon : result.photons) {
            energy_sum += photon.kinetic_energy_MeV;

            for (int axis = 0; axis < 3; ++axis) {
                momentum_sum[axis] +=
                    photon.kinetic_energy_MeV *
                    photon.direction[axis];

                direction_sum[axis] +=
                    photon.direction[axis];

                direction_square_sum[axis] +=
                    photon.direction[axis] *
                    photon.direction[axis];
            }

            if (photon.polarization_valid) {
                ++polarized_photon_count;
            }

            ++photon_count;
        }

        const double energy_error =
            std::abs(
                energy_sum -
                expected_total_energy_MeV
            );

        const double momentum_error =
            Norm(momentum_sum);

        energy_error_sum += energy_error;
        momentum_error_sum += momentum_error;

        maximum_energy_error =
            std::max(
                maximum_energy_error,
                energy_error
            );

        maximum_momentum_error =
            std::max(
                maximum_momentum_error,
                momentum_error
            );

        delay_sum += result.delay_ns;
    }

    std::cout
        << std::fixed
        << std::setprecision(10);

    std::cout
        << "=== Geant4 Ore-Powell smoke test ===\n";

    std::cout
        << "Events: " << event_count << "\n";

    std::cout
        << "Photons: " << photon_count << "\n";

    std::cout
        << "Model: " << model.Name() << "\n";

    std::cout
        << "Version: " << model.Version() << "\n";

    std::cout
        << "Validation status: "
        << model.ValidationStatus()
        << "\n\n";

    std::cout
        << "Mean delay (ns): "
        << delay_sum /
           static_cast<double>(event_count)
        << "\n";

    std::cout
        << "Mean energy error (MeV): "
        << energy_error_sum /
           static_cast<double>(event_count)
        << "\n";

    std::cout
        << "Maximum energy error (MeV): "
        << maximum_energy_error
        << "\n";

    std::cout
        << "Mean momentum closure error (MeV/c): "
        << momentum_error_sum /
           static_cast<double>(event_count)
        << "\n";

    std::cout
        << "Maximum momentum closure error (MeV/c): "
        << maximum_momentum_error
        << "\n\n";

    for (int axis = 0; axis < 3; ++axis) {
        const double mean_direction =
            direction_sum[axis] /
            static_cast<double>(photon_count);

        const double mean_direction_square =
            direction_square_sum[axis] /
            static_cast<double>(photon_count);

        std::cout
            << "Axis " << axis
            << ": mean=" << mean_direction
            << "  mean_square="
            << mean_direction_square
            << "\n";
    }

    std::cout
        << "\nPolarized photons: "
        << polarized_photon_count
        << " / "
        << photon_count
        << "\n";

    return 0;
}
