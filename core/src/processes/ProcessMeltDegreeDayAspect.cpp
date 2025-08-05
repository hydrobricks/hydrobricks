#include "ProcessMeltDegreeDayAspect.h"

#include "Brick.h"
#include "HydroUnit.h"
#include "WaterContainer.h"

ProcessMeltDegreeDayAspect::ProcessMeltDegreeDayAspect(WaterContainer* container)
    : ProcessMelt(container),
      _temperature(nullptr),
      _degreeDayFactor(nullptr),
      _meltingTemperature(nullptr) {}

void ProcessMeltDegreeDayAspect::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("degree_day_factor_n", 3);
    modelSettings->AddProcessParameter("degree_day_factor_s", 3);
    modelSettings->AddProcessParameter("degree_day_factor_ew", 3);
    modelSettings->AddProcessParameter("melting_temperature", 0);
    modelSettings->AddProcessForcing("temperature");
}

bool ProcessMeltDegreeDayAspect::IsOk() {
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

void ProcessMeltDegreeDayAspect::SetHydroUnitProperties(HydroUnit* unit, Brick*) {
    _aspectClass = unit->GetPropertyString("aspect_class");
}

void ProcessMeltDegreeDayAspect::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _meltingTemperature = GetParameterValuePointer(processSettings, "melting_temperature");

    if (_aspectClass == "north" || _aspectClass == "N") {
        _degreeDayFactor = GetParameterValuePointer(processSettings, "degree_day_factor_n");
    } else if (_aspectClass == "south" || _aspectClass == "S") {
        _degreeDayFactor = GetParameterValuePointer(processSettings, "degree_day_factor_s");
    } else if (_aspectClass == "east" || _aspectClass == "west" || _aspectClass == "E" || _aspectClass == "W") {
        if (HasParameter(processSettings, "degree_day_factor_ew")) {
            _degreeDayFactor = GetParameterValuePointer(processSettings, "degree_day_factor_ew");
        } else if (HasParameter(processSettings, "degree_day_factor_we")) {
            _degreeDayFactor = GetParameterValuePointer(processSettings, "degree_day_factor_we");
        } else {
            throw InvalidArgument("Missing parameter 'degree_day_factor_ew' or 'degree_day_factor_we'");
        }
    } else {
        throw InvalidArgument("Invalid aspect: " + _aspectClass);
    }
}

void ProcessMeltDegreeDayAspect::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == Temperature) {
        _temperature = forcing;
    } else {
        throw InvalidArgument("Forcing must be of type Temperature");
    }
}

vecDouble ProcessMeltDegreeDayAspect::GetRates() {
    if (!_container->ContentAccessible()) {
        return {0};
    }

    double melt = 0;
    if (_temperature->GetValue() >= *_meltingTemperature) {
        melt = (_temperature->GetValue() - *_meltingTemperature) * *_degreeDayFactor;
    }

    return {melt};
}
