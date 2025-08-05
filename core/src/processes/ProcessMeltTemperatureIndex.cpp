#include "ProcessMeltTemperatureIndex.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessMeltTemperatureIndex::ProcessMeltTemperatureIndex(WaterContainer* container)
    : ProcessMelt(container),
      _temperature(nullptr),
      _potentialClearSkyDirectSolarRadiation(nullptr),
      _meltFactor(nullptr),
      _meltingTemperature(nullptr),
      _radiationCoefficient(nullptr) {}

void ProcessMeltTemperatureIndex::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("melt_factor", 3.0f);
    modelSettings->AddProcessParameter("melting_temperature", 0.0f);
    modelSettings->AddProcessParameter("radiation_coefficient", 0.0007f);
    modelSettings->AddProcessForcing("temperature");
    modelSettings->AddProcessForcing("solar_radiation");
}

bool ProcessMeltTemperatureIndex::IsOk() {
    if (!ProcessMelt::IsOk()) {
        return false;
    }
    if (_temperature == nullptr) {
        return false;
    }
    if (_meltFactor == nullptr) {
        return false;
    }
    if (_meltingTemperature == nullptr) {
        return false;
    }
    if (_radiationCoefficient == nullptr) {
        return false;
    }
    if (_potentialClearSkyDirectSolarRadiation == nullptr) {
        return false;
    }

    return true;
}

void ProcessMeltTemperatureIndex::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _meltFactor = GetParameterValuePointer(processSettings, "melt_factor");
    _meltingTemperature = GetParameterValuePointer(processSettings, "melting_temperature");
    _radiationCoefficient = GetParameterValuePointer(processSettings, "radiation_coefficient");
}

void ProcessMeltTemperatureIndex::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == Temperature) {
        _temperature = forcing;
    } else if (forcing->GetType() == Radiation) {
        _potentialClearSkyDirectSolarRadiation = forcing;
    } else {
        throw InvalidArgument("Forcing must be of type Temperature or Radiation");
    }
}

vecDouble ProcessMeltTemperatureIndex::GetRates() {
    if (!_container->ContentAccessible()) {
        return {0};
    }

    double melt = 0;
    if (_temperature->GetValue() >= *_meltingTemperature) {
        melt = (_temperature->GetValue() - *_meltingTemperature) *
               (*_meltFactor + *_radiationCoefficient * _potentialClearSkyDirectSolarRadiation->GetValue());
    }

    return {melt};
}
