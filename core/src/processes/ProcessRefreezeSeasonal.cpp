#include "ProcessRefreezeSeasonal.h"

#include <cmath>
#include <numbers>

#include "TimeMachine.h"
#include "WaterContainer.h"

ProcessRefreezeSeasonal::ProcessRefreezeSeasonal(WaterContainer* container)
    : ProcessRefreezeDegreeDay(container),
      _degreeDayFactorMin(nullptr),
      _degreeDayFactorMax(nullptr),
      _meltingTemperature(nullptr) {}

void ProcessRefreezeSeasonal::RegisterProcessSettings(SettingsModel* modelSettings) {
    ProcessRefreezeDegreeDay::RegisterProcessSettings(modelSettings);
    modelSettings->AddProcessParameter("degree_day_factor_min", 2.0f);
    modelSettings->AddProcessParameter("degree_day_factor_max", 6.0f);
    modelSettings->AddProcessParameter("melting_temperature", 0.0f);
}

bool ProcessRefreezeSeasonal::IsValid() const {
    // Deliberately does NOT require a melt:degree_day sibling (unlike the base class):
    // the seasonal factor is self-contained.
    if (!ProcessTransform::IsValid()) {
        return false;
    }
    if (_temperature == nullptr) {
        LogError("Seasonal refreeze process requires temperature forcing.");
        return false;
    }
    if (_refreezingFactor == nullptr) {
        LogError("Seasonal refreeze process: missing the 'refreezing_factor' parameter.");
        return false;
    }
    if (_degreeDayFactorMin == nullptr || _degreeDayFactorMax == nullptr) {
        LogError("Seasonal refreeze process: missing the seasonal degree-day factors.");
        return false;
    }
    if (*_degreeDayFactorMin > *_degreeDayFactorMax) {
        LogError("Seasonal refreeze process: 'degree_day_factor_min' exceeds 'degree_day_factor_max'.");
        return false;
    }
    if (_meltingTemperature == nullptr) {
        LogError("Seasonal refreeze process: missing the 'melting_temperature' parameter.");
        return false;
    }

    return true;
}

void ProcessRefreezeSeasonal::SetParameters(const ProcessSettings& processSettings) {
    ProcessRefreezeDegreeDay::SetParameters(processSettings);
    _degreeDayFactorMin = GetParameterValuePointer(processSettings, "degree_day_factor_min");
    _degreeDayFactorMax = GetParameterValuePointer(processSettings, "degree_day_factor_max");
    _meltingTemperature = GetParameterValuePointer(processSettings, "melting_temperature");
}

const vecDouble& ProcessRefreezeSeasonal::GetRates() {
    if (_temperature->GetValue() >= *_meltingTemperature) {
        return StoreRates({0});
    }

    double mean = ((*_degreeDayFactorMax) + (*_degreeDayFactorMin)) / 2.0;
    double amplitude = ((*_degreeDayFactorMax) - (*_degreeDayFactorMin)) / 2.0;
    double seasonalFactor = 0;
    if (_timeMachine != nullptr) {
        int doy = _timeMachine->GetCurrentDayOfYear();
        seasonalFactor = std::sin(2.0 * std::numbers::pi * (doy - 80) / 366.0);
    }
    double ddf = mean + amplitude * seasonalFactor;

    double refreeze = (*_refreezingFactor) * ddf * (*_meltingTemperature - _temperature->GetValue());

    return StoreRates({refreeze});
}
