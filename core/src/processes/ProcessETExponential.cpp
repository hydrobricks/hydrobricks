#include "ProcessETExponential.h"

#include <cmath>

#include "Brick.h"
#include "WaterContainer.h"

ProcessETExponential::ProcessETExponential(WaterContainer* container)
    : ProcessET(container),
      _pet(nullptr),
      _alpha(nullptr) {}

void ProcessETExponential::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("alpha", 2.0f);
    modelSettings->AddProcessForcing("pet");
}

bool ProcessETExponential::IsValid() const {
    if (!ProcessET::IsValid()) {
        return false;
    }
    if (_pet == nullptr) {
        LogError("Exponential ET process requires PET forcing.");
        return false;
    }
    if (_alpha == nullptr) {
        LogError("Exponential ET process: missing the 'alpha' parameter.");
        return false;
    }

    return true;
}

void ProcessETExponential::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _alpha = GetParameterValuePointer(processSettings, "alpha");
}

void ProcessETExponential::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::PET) {
        _pet = forcing;
    } else {
        throw ModelConfigError("Exponential ET: forcing must be of type PET.");
    }
}

const vecDouble& ProcessETExponential::GetRates() {
    assert(_container->HasMaximumCapacity());
    double ratio = _container->GetTargetFillingRatio();
    return StoreRates({_pet->GetValue() * (1.0 - exp(-static_cast<double>(*_alpha) * ratio))});
}
