#include "ConfigurablePsModel.hh"

#include <stdexcept>
#include <utility>

ConfigurablePsModel::ConfigurablePsModel()
    : m_ore_powell_model(
          OrePowellPsModel::PolarizationMode::Random
      ),
      m_polarized_ore_powell_model(
          OrePowellPsModel::PolarizationMode::Polarized
      )
{
}

void ConfigurablePsModel::SetThreeGammaModel(
    ThreeGammaModel model
)
{
    m_three_gamma_model = model;
}

ConfigurablePsModel::ThreeGammaModel
ConfigurablePsModel::GetThreeGammaModel() const
{
    return m_three_gamma_model;
}

void ConfigurablePsModel::SetDelayMode(
    DelayMode mode
)
{
    m_parameterized_model.SetDelayMode(mode);
}

void ConfigurablePsModel::SetFixedDelayNs(
    double delay_ns
)
{
    m_parameterized_model.SetFixedDelayNs(delay_ns);
}

void ConfigurablePsModel::SetEnableThreeGamma(
    bool enabled
)
{
    m_parameterized_model.SetEnableThreeGamma(enabled);
}

PsModelResult ConfigurablePsModel::Sample(
    const PsEnvironment& environment
) const
{

    const PsBranchResult branch =
        m_parameterized_model.SampleBranch(
            environment
        );

    PsModelResult result;

    result.ps_class =
        branch.ps_class;

    result.annihilation_mode =
        branch.annihilation_mode;

    result.delay_ns =
        branch.delay_ns;

    // The parameterized model controls branch selection and lifetime.
    // Only replace the photon kinematics when a 3-gamma branch is selected.
    if (result.annihilation_mode != 3) {
        result.photons =
            m_parameterized_model.SamplePhotons(
                result.ps_class
            );

        result.model_name = Name();
        result.model_version = Version();
        result.validation_status =
            ValidationStatus();

        return result;
    }

    if (result.ps_class != PsClass::OrthoPs3g) {
        throw std::runtime_error(
            "A 3-gamma event was returned without an "
            "OrthoPs3g branch classification."
        );
    }

    switch (m_three_gamma_model) {
        case ThreeGammaModel::ApproximatePhaseSpace:
            result.photons =
                m_parameterized_model.SamplePhotons(
                    result.ps_class
                );
            break;

        case ThreeGammaModel::Geant4OrePowell: {
            PsModelResult ore_powell_result =
                m_ore_powell_model.Sample(environment);

            result.photons =
                std::move(ore_powell_result.photons);

            break;
        }

        case ThreeGammaModel::Geant4PolarizedOrePowell: {
            PsModelResult ore_powell_result =
                m_polarized_ore_powell_model.Sample(
                    environment
                );

            result.photons =
                std::move(ore_powell_result.photons);

            break;
        }
    }

    if (result.photons.size() != 3) {
        throw std::runtime_error(
            "Selected 3-gamma model did not return "
            "exactly three photons."
        );
    }

    result.model_name = Name();
    result.model_version = Version();
    result.validation_status = ValidationStatus();

    return result;
}

std::string ConfigurablePsModel::Name() const
{
    switch (m_three_gamma_model) {
        case ThreeGammaModel::ApproximatePhaseSpace:
            return
                "ConfigurablePsModel/"
                "ApproximatePhaseSpace";

        case ThreeGammaModel::Geant4OrePowell:
            return
                "ConfigurablePsModel/"
                "Geant4OrePowell";

        case ThreeGammaModel::Geant4PolarizedOrePowell:
            return
                "ConfigurablePsModel/"
                "Geant4PolarizedOrePowell";
    }

    return "ConfigurablePsModel/Unknown";
}

std::string ConfigurablePsModel::Version() const
{
    switch (m_three_gamma_model) {
        case ThreeGammaModel::ApproximatePhaseSpace:
            return "1.0";

        case ThreeGammaModel::Geant4OrePowell:
        case ThreeGammaModel::Geant4PolarizedOrePowell:
            return "Geant4-11.3.2";
    }

    return "unknown";
}

std::string ConfigurablePsModel::ValidationStatus() const
{
    switch (m_three_gamma_model) {
        case ThreeGammaModel::ApproximatePhaseSpace:
            return
                "approximate-controlled-source-model";

        case ThreeGammaModel::Geant4OrePowell:
        case ThreeGammaModel::Geant4PolarizedOrePowell:
            return
                "geant4-native-ore-powell";
    }

    return "unknown";
}
