#include "ProcessMeltDegreeDay.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessMeltDegreeDay::ProcessMeltDegreeDay(WaterContainer* container)
    : ProcessMelt(container),
      _temperature(nullptr),
      _degreeDayFactor(nullptr),
      _meltingTemperature(nullptr) {}

void ProcessMeltDegreeDay::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("degree_day_factor", 3.0f);
    modelSettings->AddProcessParameter("melting_temperature", 0.0f);
    modelSettings->AddProcessForcing("temperature");
}

bool ProcessMeltDegreeDay::IsOk() {
    if (!ProcessMelt::IsOk()) {
        return false;
    }
    if (_temperature == nullptr) {
        return false;
    }
    if (_degreeDayFactor == nullptr) {
        return false;
    }
    if (_meltingTemperature == nullptr) {
        return false;
    }

    return true;
}

void ProcessMeltDegreeDay::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _degreeDayFactor = GetParameterValuePointer(processSettings, "degree_day_factor");
    _meltingTemperature = GetParameterValuePointer(processSettings, "melting_temperature");
}

void ProcessMeltDegreeDay::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == Temperature) {
        _temperature = forcing;
    } else {
        throw InvalidArgument("Forcing must be of type Temperature");
    }
}

vecDouble ProcessMeltDegreeDay::GetRates() {
    if (!_container->ContentAccessible()) {
        return {0};
    }

    double melt = 0;
    if (_temperature->GetValue() >= *_meltingTemperature) {
        melt = (_temperature->GetValue() - *_meltingTemperature) * *_degreeDayFactor;
    }

    return {melt};
}
