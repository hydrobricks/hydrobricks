#include "ProcessETOpenWater.h"

#include "WaterContainer.h"

ProcessETOpenWater::ProcessETOpenWater(WaterContainer* container)
    : ProcessET(container),
      _pet(nullptr) {}

void ProcessETOpenWater::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessForcing("pet");
}

bool ProcessETOpenWater::IsValid() const {
    if (!ProcessET::IsValid()) {
        return false;
    }
    if (_pet == nullptr) {
        LogError("Open water ET process requires PET forcing.");
        return false;
    }

    return true;
}

void ProcessETOpenWater::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::PET) {
        _pet = forcing;
    } else {
        throw ModelConfigError("Open water ET: forcing must be PET");
    }
}

vecDouble ProcessETOpenWater::GetRates() {
    // Potential rate; the water container limits the actual loss to the content.
    return {_pet->GetValue()};
}
