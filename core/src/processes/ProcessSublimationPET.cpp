#include "ProcessSublimationPET.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessSublimationPET::ProcessSublimationPET(WaterContainer* container)
    : ProcessSublimation(container),
      _pet(nullptr),
      _petFactor(nullptr) {}

void ProcessSublimationPET::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("sublimation_pet_factor", 0.2f);
    modelSettings->AddProcessForcing("pet");
}

bool ProcessSublimationPET::IsValid() const {
    if (!ProcessSublimation::IsValid()) {
        return false;
    }
    if (_pet == nullptr) {
        LogError("PET-fraction sublimation requires PET forcing.");
        return false;
    }
    if (_petFactor == nullptr) {
        return false;
    }

    return true;
}

void ProcessSublimationPET::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _petFactor = GetParameterValuePointer(processSettings, "sublimation_pet_factor");
}

void ProcessSublimationPET::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::PET) {
        _pet = forcing;
    } else {
        throw ModelConfigError("PET-fraction sublimation: forcing must be PET");
    }
}

const vecDouble& ProcessSublimationPET::GetRates() {
    if (!_container->ContentAccessible()) {
        return StoreRates({0});
    }

    return StoreRates({_pet->GetValue() * *_petFactor});
}
