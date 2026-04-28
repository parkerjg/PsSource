#include "PositroniumProvider.hh"
#include "TimedEventModel.hh"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

static double Norm3(const std::array<double,3>& v)
{
    return std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

int main(int argc, char** argv)
{
    int n = 10;
    if (argc > 1) {
        n = std::stoi(argv[1]);
    }

    PositroniumProvider p;
    p.SetFractions(0.0, 0.0, 1.0);
    p.SetEnableThreeGamma(true);
    p.SetOrthoThreeGammaFraction(1.0);
    p.SetDelayMode(PositroniumProvider::DelayMode::Exponential);
    p.SetTauOpsNs(3.0);
    p.SetHasPromptGamma(false);
    p.SetEnablePositronRange(false);
    p.SetEnableQuantumEntanglement(true);

    std::map<int,int> mode_counts;
    std::map<int,int> class_counts;

    std::cout << std::fixed << std::setprecision(6);

    for (int i = 0; i < n; ++i) {
        TimedEventSpec ev = p.SampleNextEvent();

        if (ev.vertices.empty()) {
            throw std::runtime_error("SampleNextEvent returned zero vertices.");
        }

        const VertexSpec& ann = ev.vertices.back();

        double esum = 0.0;
        std::array<double,3> psum{0.0, 0.0, 0.0};

        for (const auto& part : ann.particles) {
            esum += part.kinetic_energy_MeV;
            psum[0] += part.kinetic_energy_MeV * part.direction[0];
            psum[1] += part.kinetic_energy_MeV * part.direction[1];
            psum[2] += part.kinetic_energy_MeV * part.direction[2];
        }

        mode_counts[ev.annihilation_mode]++;
        class_counts[ev.ps_class_id]++;

        std::cout
            << "event " << i
            << "  ps_class=" << ev.ps_class_id
            << "  mode=" << ev.annihilation_mode
            << "  delay_ns=" << ev.delay_ns
            << "  n_gamma=" << ann.particles.size()
            << "  E=[";
        for (std::size_t j = 0; j < ann.particles.size(); ++j) {
            if (j) std::cout << ", ";
            std::cout << ann.particles[j].kinetic_energy_MeV;
        }
        std::cout
            << "]"
            << "  Esum=" << esum
            << "  |psum|=" << Norm3(psum)
            << "\n";
    }

    std::cout << "\nSummary\n";
    for (const auto& kv : class_counts) {
        std::cout << "  ps_class " << kv.first << " : " << kv.second << "\n";
    }
    for (const auto& kv : mode_counts) {
        std::cout << "  annihilation_mode " << kv.first << " : " << kv.second << "\n";
    }

    return 0;
}
