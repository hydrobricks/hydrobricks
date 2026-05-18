#include "ProcessETSocont.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessETSocont::ProcessETSocont(WaterContainer* container)
    : ProcessET(container),
      _pet(nullptr),
      _exponent(0.5) {}

void ProcessETSocont::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessForcing("pet");
}

bool ProcessETSocont::IsValid() const {
    if (!ProcessET::IsValid()) {
        return false;
    }
    if (_pet == nullptr) {
        LogError("Socont ET process requires PET forcing.");
        return false;
    }

    return true;
}

void ProcessETSocont::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::PET) {
        _pet = forcing;
    } else {
        throw ModelConfigError("Socont ET: forcing must be PET");
    }
}

vecDouble ProcessETSocont::GetRates() {
    assert(_container->HasMaximumCapacity());
    return {_pet->GetValue() * pow(_container->GetTargetFillingRatio(), _exponent)};
}
