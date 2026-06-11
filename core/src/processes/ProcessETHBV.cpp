#include "ProcessETHBV.h"

#include <algorithm>

#include "WaterContainer.h"

ProcessETHBV::ProcessETHBV(WaterContainer* container)
    : ProcessET(container),
      _pet(nullptr),
      _lp(nullptr) {}

void ProcessETHBV::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("lp", 0.9f);
    modelSettings->AddProcessForcing("pet");
}

bool ProcessETHBV::IsValid() const {
    if (!ProcessET::IsValid()) {
        return false;
    }
    if (_pet == nullptr) {
        LogError("HBV ET process requires PET forcing.");
        return false;
    }
    if (_lp == nullptr) {
        LogError("HBV ET process: missing the 'lp' parameter.");
        return false;
    }

    return true;
}

void ProcessETHBV::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _lp = GetParameterValuePointer(processSettings, "lp");
}

void ProcessETHBV::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::PET) {
        _pet = forcing;
    } else {
        throw ModelConfigError("HBV ET: forcing must be of type PET.");
    }
}

vecDouble ProcessETHBV::GetRates() {
    assert(_container->HasMaximumCapacity());
    double threshold = static_cast<double>(*_lp) * _container->GetMaximumCapacity();
    if (threshold <= 0) {
        return {_pet->GetValue()};
    }

    double ratio = std::min(1.0, _container->GetContentWithChanges() / threshold);

    return {_pet->GetValue() * ratio};
}
