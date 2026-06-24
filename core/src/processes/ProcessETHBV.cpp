#include "ProcessETHBV.h"

#include <algorithm>

#include "WaterContainer.h"

ProcessETHBV::ProcessETHBV(WaterContainer* container)
    : ProcessET(container),
      _pet(nullptr),
      _lp(nullptr),
      _etCorrectionFactor(nullptr) {}

void ProcessETHBV::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("lp", 0.9f);
    modelSettings->AddProcessParameter("et_correction_factor", 1.0f);
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
    if (_etCorrectionFactor == nullptr) {
        LogError("HBV ET process: missing the 'et_correction_factor' parameter.");
        return false;
    }

    return true;
}

void ProcessETHBV::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _lp = GetParameterValuePointer(processSettings, "lp");
    _etCorrectionFactor = GetParameterValuePointer(processSettings, "et_correction_factor");
}

void ProcessETHBV::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::PET) {
        _pet = forcing;
    } else {
        throw ModelConfigError("HBV ET: forcing must be of type PET.");
    }
}

const vecDouble& ProcessETHBV::GetRates() {
    assert(_container->HasMaximumCapacity());
    double pet = static_cast<double>(*_etCorrectionFactor) * _pet->GetValue();
    double threshold = static_cast<double>(*_lp) * _container->GetMaximumCapacity();
    if (threshold <= 0) {
        return StoreRates({pet});
    }

    double ratio = std::min(1.0, _container->GetContentWithChanges() / threshold);

    return StoreRates({pet * ratio});
}
