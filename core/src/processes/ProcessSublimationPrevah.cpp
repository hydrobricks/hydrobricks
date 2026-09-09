#include "ProcessSublimationPrevah.h"

#include "HydroUnit.h"
#include "Snowpack.h"
#include "WaterContainer.h"

ProcessSublimationPrevah::ProcessSublimationPrevah(WaterContainer* container)
    : ProcessSublimation(container),
      _pet(nullptr),
      _snowpack(nullptr) {}

void ProcessSublimationPrevah::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessForcing("pet");
}

bool ProcessSublimationPrevah::IsValid() const {
    if (!ProcessSublimation::IsValid()) {
        return false;
    }
    if (_pet == nullptr) {
        LogError("PREVAH snow evaporation requires PET forcing.");
        return false;
    }
    if (_hydroUnit == nullptr) {
        LogError("PREVAH snow evaporation requires a hydro-unit snowpack context.");
        return false;
    }

    return true;
}

void ProcessSublimationPrevah::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::PET) {
        _pet = forcing;
    } else {
        throw ModelConfigError("PREVAH snow evaporation: forcing must be PET");
    }
}

const vecDouble& ProcessSublimationPrevah::GetRates() {
    if (!_container->ContentAccessible()) {
        return StoreRates({0});
    }

    // Resolve the owning snowpack once (needed for its age-dependent snow albedo).
    if (_snowpack == nullptr) {
        _snowpack = dynamic_cast<Snowpack*>(_hydroUnit->TryGetBrick(_parentBrickName));
        if (_snowpack == nullptr) {
            return StoreRates({0});
        }
    }

    // Snow evaporates at the albedo-reduced potential rate (albedo of the snow surface).
    double albedoFactor = (1.0 - _snowpack->GetSnowAlbedo()) / 0.8;

    return StoreRates({_pet->GetValue() * albedoFactor});
}
