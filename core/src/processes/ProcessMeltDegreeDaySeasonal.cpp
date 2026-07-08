#include "ProcessMeltDegreeDaySeasonal.h"

#include <cmath>
#include <numbers>

#include "Brick.h"
#include "TimeMachine.h"
#include "WaterContainer.h"

ProcessMeltDegreeDaySeasonal::ProcessMeltDegreeDaySeasonal(WaterContainer* container)
    : ProcessMeltDegreeDay(container),
      _degreeDayFactorMin(nullptr),
      _degreeDayFactorMax(nullptr) {}

void ProcessMeltDegreeDaySeasonal::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("degree_day_factor_min", 2.0f);
    modelSettings->AddProcessParameter("degree_day_factor_max", 6.0f);
    modelSettings->AddProcessParameter("melting_temperature", 0.0f);
    modelSettings->AddProcessForcing("temperature");
}

bool ProcessMeltDegreeDaySeasonal::IsValid() const {
    if (!ProcessMelt::IsValid()) {
        return false;
    }
    if (_temperature == nullptr) {
        return false;
    }
    if (_degreeDayFactorMin == nullptr) {
        return false;
    }
    if (_degreeDayFactorMax == nullptr) {
        return false;
    }
    if (*_degreeDayFactorMin > *_degreeDayFactorMax) {
        LogError("Seasonal degree-day melt process: 'degree_day_factor_min' exceeds 'degree_day_factor_max'.");
        return false;
    }
    if (_meltingTemperature == nullptr) {
        return false;
    }

    return true;
}

void ProcessMeltDegreeDaySeasonal::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _degreeDayFactorMin = GetParameterValuePointer(processSettings, "degree_day_factor_min");
    _degreeDayFactorMax = GetParameterValuePointer(processSettings, "degree_day_factor_max");
    _meltingTemperature = GetParameterValuePointer(processSettings, "melting_temperature");
}

double ProcessMeltDegreeDaySeasonal::GetDegreeDayFactor() const {
    double mean = ((*_degreeDayFactorMax) + (*_degreeDayFactorMin)) / 2.0;
    double amplitude = ((*_degreeDayFactorMax) - (*_degreeDayFactorMin)) / 2.0;

    // Seasonal sine: zero at the spring equinox (doy 80), maximum at the summer
    // solstice. Without a time machine (unit tests) the mean factor is used.
    double seasonalFactor = 0;
    if (_timeMachine != nullptr) {
        int doy = _timeMachine->GetCurrentDayOfYear();
        seasonalFactor = std::sin(2.0 * std::numbers::pi * (doy - 80) / 366.0);
    }

    return mean + amplitude * seasonalFactor;
}

const vecDouble& ProcessMeltDegreeDaySeasonal::GetRates() {
    if (!_container->ContentAccessible()) {
        return StoreRates({0});
    }

    double melt = 0;
    if (_temperature->GetValue() >= *_meltingTemperature) {
        melt = (_temperature->GetValue() - *_meltingTemperature) * GetDegreeDayFactor();
    }

    return StoreRates({melt});
}
