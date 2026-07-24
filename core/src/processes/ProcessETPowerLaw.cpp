#include "ProcessETPowerLaw.h"

#include <cmath>

#include "Brick.h"
#include "WaterContainer.h"

ProcessETPowerLaw::ProcessETPowerLaw(WaterContainer* container)
    : ProcessET(container),
      _pet(nullptr),
      _exponent(nullptr) {}

void ProcessETPowerLaw::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("exponent", 0.5f);
    modelSettings->AddProcessForcing("pet");
}

bool ProcessETPowerLaw::IsValid() const {
    if (!ProcessET::IsValid()) {
        return false;
    }
    if (_pet == nullptr) {
        LogError("Power-law ET process requires PET forcing.");
        return false;
    }
    if (_exponent == nullptr) {
        LogError("Power-law ET process: missing the 'exponent' parameter.");
        return false;
    }

    return true;
}

void ProcessETPowerLaw::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _exponent = GetParameterValuePointer(processSettings, "exponent");
}

void ProcessETPowerLaw::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::PET) {
        _pet = forcing;
    } else {
        throw ModelConfigError("Power-law ET: forcing must be of type PET.");
    }
}

const vecDouble& ProcessETPowerLaw::GetRates() {
    assert(_container->HasMaximumCapacity());
    return StoreRates({_pet->GetValue() * pow(_container->GetTargetFillingRatio(), static_cast<double>(*_exponent))});
}
