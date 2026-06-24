#include "ProcessETLinear.h"

#include <algorithm>

#include "Brick.h"
#include "WaterContainer.h"

ProcessETLinear::ProcessETLinear(WaterContainer* container)
    : ProcessET(container),
      _pet(nullptr) {}

void ProcessETLinear::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessForcing("pet");
}

bool ProcessETLinear::IsValid() const {
    if (!ProcessET::IsValid()) {
        return false;
    }
    if (_pet == nullptr) {
        LogError("Linear ET process requires PET forcing.");
        return false;
    }

    return true;
}

void ProcessETLinear::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::PET) {
        _pet = forcing;
    } else {
        throw ModelConfigError("Linear ET: forcing must be of type PET.");
    }
}

const vecDouble& ProcessETLinear::GetRates() {
    assert(_container->HasMaximumCapacity());
    return StoreRates({_pet->GetValue() * std::min(1.0, _container->GetTargetFillingRatio())});
}
