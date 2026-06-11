#include "ProcessInfiltrationHBV.h"

#include <algorithm>
#include <cmath>

#include "WaterContainer.h"

ProcessInfiltrationHBV::ProcessInfiltrationHBV(WaterContainer* container)
    : ProcessInfiltration(container),
      _beta(nullptr) {}

void ProcessInfiltrationHBV::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("beta", 2.0f);
}

bool ProcessInfiltrationHBV::IsValid() const {
    if (!ProcessInfiltration::IsValid()) {
        return false;
    }
    if (_beta == nullptr) {
        LogError("HBV infiltration process: missing the 'beta' parameter.");
        return false;
    }

    return true;
}

void ProcessInfiltrationHBV::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _beta = GetParameterValuePointer(processSettings, "beta");
}

vecDouble ProcessInfiltrationHBV::GetRates() {
    double in = _container->GetContentWithChanges();  // rain + snowpack outflow on the land cover
    double fc = GetTargetCapacity();                  // soil moisture capacity (FC)
    if (in <= 0 || fc <= 0) {
        return {0};
    }

    double ratio = std::clamp(GetTargetFillingRatio(), 0.0, 1.0);

    return {in * (1.0 - std::pow(ratio, static_cast<double>(*_beta)))};
}
