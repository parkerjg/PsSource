#include "OrePowellPsModel.hh"

#include "G4DynamicParticle.hh"
#include "G4Material.hh"
#include "G4OrePowellAtRestModel.hh"
#include "G4PolarizedOrePowellAtRestModel.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4VPositronAtRestModel.hh"

#include "Randomize.hh"

#include <cmath>
#include <stdexcept>
#include <vector>

OrePowellPsModel::OrePowellPsModel(
    PolarizationMode polarization_mode
)
    : m_polarization_mode(polarization_mode)
{
    if (
        m_polarization_mode ==
        PolarizationMode::Polarized
    ) {
        m_model =
            std::make_unique<
                G4PolarizedOrePowellAtRestModel
            >();
    } else {
        m_model =
            std::make_unique<
                G4OrePowellAtRestModel
            >();
    }
}

OrePowellPsModel::~OrePowellPsModel() = default;

void OrePowellPsModel::SetMaterial(
    const G4Material* material
)
{
    m_material = material;
}

PsModelResult OrePowellPsModel::Sample(
    const PsEnvironment& environment
) const
{
    if (!m_model) {
        throw std::runtime_error(
            "Ore-Powell Geant4 model is not initialized."
        );
    }

    if (environment.tau_ops_ns <= 0.0) {
        throw std::invalid_argument(
            "Ortho-Ps lifetime must be strictly positive."
        );
    }

    std::vector<G4DynamicParticle*> secondaries;
    G4double local_energy_deposit = 0.0;

    m_model->SampleSecondaries(
        secondaries,
        local_energy_deposit,
        m_material
    );

    if (secondaries.size() != 3) {
        for (G4DynamicParticle* particle : secondaries) {
            delete particle;
        }

        throw std::runtime_error(
            "Geant4 Ore-Powell model did not return exactly "
            "three secondary particles."
        );
    }

    PsModelResult result;

    result.ps_class = PsClass::OrthoPs3g;
    result.annihilation_mode = 3;

    double uniform_sample = 0.0;

    do {
        uniform_sample = G4UniformRand();
    } while (
        uniform_sample <= 0.0 ||
        uniform_sample >= 1.0
    );

    result.delay_ns =
        -environment.tau_ops_ns *
        std::log(uniform_sample);

    result.photons.reserve(3);

    for (G4DynamicParticle* particle : secondaries) {
        if (!particle) {
            continue;
        }

        PsPhoton photon;

        photon.kinetic_energy_MeV =
            particle->GetKineticEnergy() / MeV;

        const G4ThreeVector direction =
            particle->GetMomentumDirection();

        photon.direction = {
            direction.x(),
            direction.y(),
            direction.z()
        };

	if (
	    m_polarization_mode ==
	    PolarizationMode::Polarized
	) {
	    const G4ThreeVector polarization =
		particle->GetPolarization();

	    photon.polarization = {
		polarization.x(),
		polarization.y(),
		polarization.z()
	    };

	    photon.polarization_valid =
		polarization.mag2() > 0.0;
	} else {
	    photon.polarization = {
		0.0,
		0.0,
		0.0
	    };

	    photon.polarization_valid =
		false;
	}

        result.photons.push_back(photon);

        delete particle;
    }

    if (result.photons.size() != 3) {
        throw std::runtime_error(
            "Failed to convert all Geant4 Ore-Powell photons."
        );
    }

    result.model_name = Name();
    result.model_version = Version();
    result.validation_status = ValidationStatus();

    return result;
}

std::string OrePowellPsModel::Name() const
{
    if (
        m_polarization_mode ==
        PolarizationMode::Polarized
    ) {
        return "Geant4PolarizedOrePowellAtRestModel";
    }

    return "Geant4OrePowellAtRestModel";
}

std::string OrePowellPsModel::Version() const
{
    return "Geant4-11.3.2";
}

std::string OrePowellPsModel::ValidationStatus() const
{
    return "geant4-native-ore-powell";
}
