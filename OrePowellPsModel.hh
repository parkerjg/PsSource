#ifndef ORE_POWELL_PS_MODEL_HH
#define ORE_POWELL_PS_MODEL_HH

#include "PsPhysicsModel.hh"

#include <memory>
#include <string>

class G4Material;
class G4VPositronAtRestModel;

class OrePowellPsModel : public IPsPhysicsModel {
public:
    enum class PolarizationMode {
        Random,
        Polarized
    };

    explicit OrePowellPsModel(
        PolarizationMode polarization_mode =
            PolarizationMode::Polarized
    );

    ~OrePowellPsModel() override;

    OrePowellPsModel(const OrePowellPsModel&) = delete;
    OrePowellPsModel& operator=(const OrePowellPsModel&) = delete;

    void SetMaterial(const G4Material* material);

    PsModelResult Sample(
        const PsEnvironment& environment
    ) const override;

    std::string Name() const override;
    std::string Version() const override;
    std::string ValidationStatus() const override;

private:
    PolarizationMode m_polarization_mode;
    std::unique_ptr<G4VPositronAtRestModel> m_model;
    const G4Material* m_material = nullptr;
};

#endif
