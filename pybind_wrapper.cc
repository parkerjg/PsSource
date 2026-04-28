#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "PositroniumGenerator.hh"
#include "PositroniumProvider.hh"

namespace py = pybind11;

PYBIND11_MODULE(gate_positronium, m)
{
    m.doc() = "Pybind11 bindings for configurable positronium source generation";

    // -------------------------------------------------------------------------
    // Enums
    // -------------------------------------------------------------------------
    py::enum_<PositroniumProvider::DelayMode>(m, "DelayMode")
        .value("Fixed", PositroniumProvider::DelayMode::Fixed)
        .value("Exponential", PositroniumProvider::DelayMode::Exponential)
        .export_values();

    py::enum_<PositroniumGenerator::GenerationMode>(m, "GenerationMode")
        .value("NativeGeant4", PositroniumGenerator::GenerationMode::NativeGeant4)
        .value("ExplicitProvider", PositroniumGenerator::GenerationMode::ExplicitProvider)
        .export_values();

    // -------------------------------------------------------------------------
    // Provider bindings
    // -------------------------------------------------------------------------
    py::class_<PositroniumProvider>(m, "PositroniumProvider")
        .def(py::init<>())

        // Branching / decay controls
        .def("set_fractions",
             &PositroniumProvider::SetFractions,
             py::arg("f_direct"),
             py::arg("f_pps"),
             py::arg("f_ops"))
        .def("set_ortho_three_gamma_fraction",
             &PositroniumProvider::SetOrthoThreeGammaFraction,
             py::arg("fraction"))
        .def("set_enable_three_gamma",
             &PositroniumProvider::SetEnableThreeGamma,
             py::arg("enabled"))
        .def("set_delay_mode",
             &PositroniumProvider::SetDelayMode,
             py::arg("mode"))
        .def("set_tau_para_ps_ns",
             &PositroniumProvider::SetTauParaPsNs,
             py::arg("tau_ns"))
        .def("set_tau_ops_ns",
             &PositroniumProvider::SetTauOpsNs,
             py::arg("tau_ns"))
        .def("set_fixed_delay_ns",
             &PositroniumProvider::SetFixedDelayNs,
             py::arg("delay_ns"))

        // Source controls
        .def("set_source_position",
             &PositroniumProvider::SetSourcePosition,
             py::arg("position_mm"))
        .def("set_has_prompt_gamma",
             &PositroniumProvider::SetHasPromptGamma,
             py::arg("enabled"))
        .def("set_prompt_energy_mev",
             &PositroniumProvider::SetPromptEnergyMeV,
             py::arg("energy_mev"))

        // Optional spatial / metadata controls
        .def("set_enable_positron_range",
             &PositroniumProvider::SetEnablePositronRange,
             py::arg("enabled"))
        .def("set_positron_range_sigma_mm",
             &PositroniumProvider::SetPositronRangeSigmaMm,
             py::arg("sigma_mm"))
        .def("set_enable_quantum_entanglement",
             &PositroniumProvider::SetEnableQuantumEntanglement,
             py::arg("enabled"));

    // -------------------------------------------------------------------------
    // Generator bindings
    // -------------------------------------------------------------------------
    py::class_<PositroniumGenerator>(m, "PositroniumGenerator")
        .def(py::init<>())

        // Generation mode
        .def("set_generation_mode",
             &PositroniumGenerator::SetGenerationMode,
             py::arg("mode"))
        .def("get_generation_mode",
             &PositroniumGenerator::GetGenerationMode)

        // Shared source controls
        .def("set_source_position",
             &PositroniumGenerator::SetSourcePosition,
             py::arg("position_mm"))
        .def("set_positron_kinetic_energy_mev",
             &PositroniumGenerator::SetPositronKineticEnergyMeV,
             py::arg("energy_mev"))
        .def("set_positron_kinetic_energy_kev",
             &PositroniumGenerator::SetPositronKineticEnergyKeV,
             py::arg("energy_kev"))
        .def("set_has_prompt_gamma",
             &PositroniumGenerator::SetHasPromptGamma,
             py::arg("enabled"))
        .def("set_prompt_energy_mev",
             &PositroniumGenerator::SetPromptEnergyMeV,
             py::arg("energy_mev"))
        .def("set_enable_quantum_entanglement",
             &PositroniumGenerator::SetEnableQuantumEntanglement,
             py::arg("enabled"))
        .def("set_base_time_ns",
             &PositroniumGenerator::SetBaseTimeNs,
             py::arg("time_ns"))
        .def("set_use_external_base_time",
             &PositroniumGenerator::SetUseExternalBaseTime,
             py::arg("enabled"))

        // Explicit-provider controls
        .def("set_fractions",
             &PositroniumGenerator::SetFractions,
             py::arg("f_direct"),
             py::arg("f_pps"),
             py::arg("f_ops"))
        .def("set_delay_mode",
             &PositroniumGenerator::SetDelayMode,
             py::arg("mode"))
        .def("set_tau_para_ps_ns",
             &PositroniumGenerator::SetTauParaPsNs,
             py::arg("tau_ns"))
        .def("set_tau_ops_ns",
             &PositroniumGenerator::SetTauOpsNs,
             py::arg("tau_ns"))
        .def("set_fixed_delay_ns",
             &PositroniumGenerator::SetFixedDelayNs,
             py::arg("delay_ns"))
        .def("set_enable_three_gamma",
             &PositroniumGenerator::SetEnableThreeGamma,
             py::arg("enabled"))
        .def("set_ortho_three_gamma_fraction",
             &PositroniumGenerator::SetOrthoThreeGammaFraction,
             py::arg("fraction"))
        .def("set_enable_positron_range",
             &PositroniumGenerator::SetEnablePositronRange,
             py::arg("enabled"))
        .def("set_positron_range_sigma_mm",
             &PositroniumGenerator::SetPositronRangeSigmaMm,
             py::arg("sigma_mm"));
}
