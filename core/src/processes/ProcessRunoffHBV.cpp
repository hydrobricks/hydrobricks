#include "ProcessRunoffHBV.h"

#include <cmath>

#include "WaterContainer.h"

ProcessRunoffHBV::ProcessRunoffHBV(WaterContainer* container)
    : ProcessOutflow(container),
      _responseFactor(nullptr),
      _alpha(nullptr) {}

void ProcessRunoffHBV::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("response_factor", 0.05f);
    modelSettings->AddProcessParameter("alpha", 1.0f);
}

bool ProcessRunoffHBV::IsValid() const {
    if (!ProcessOutflow::IsValid()) {
        return false;
    }
    if (_responseFactor == nullptr || _alpha == nullptr) {
        LogError("HBV runoff process: one or more parameters are missing.");
        return false;
    }

    return true;
}

void ProcessRunoffHBV::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _responseFactor = GetParameterValuePointer(processSettings, "response_factor");
    _alpha = GetParameterValuePointer(processSettings, "alpha");
}

const vecDouble& ProcessRunoffHBV::GetRates() {
    double uz = _container->GetContentWithChanges();
    if (uz <= 0) {
        return StoreRates({0});
    }

    return StoreRates({(*_responseFactor) * std::pow(uz, 1.0 + static_cast<double>(*_alpha))});
}
