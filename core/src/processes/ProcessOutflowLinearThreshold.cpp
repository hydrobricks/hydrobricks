#include "ProcessOutflowLinearThreshold.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflowLinearThreshold::ProcessOutflowLinearThreshold(WaterContainer* container)
    : ProcessOutflow(container),
      _responseFactor(nullptr),
      _threshold(nullptr) {}

void ProcessOutflowLinearThreshold::RegisterProcessSettings(SettingsModel* modelSettings) {
    // The response factor carries a distinct name (not 'response_factor') so the process
    // can share a brick with an 'outflow:linear' (e.g. the PREVAH upper zone) while both
    // parameters stay addressable through the component:name lookup.
    modelSettings->AddProcessParameter("response_factor_threshold", 0.2f);
    modelSettings->AddProcessParameter("threshold", 0.0f);
}

bool ProcessOutflowLinearThreshold::IsValid() const {
    if (!ProcessOutflow::IsValid()) {
        return false;
    }
    if (_responseFactor == nullptr) {
        LogError("Linear threshold outflow process: missing the 'response_factor_threshold' parameter.");
        return false;
    }
    if (_threshold == nullptr) {
        LogError("Linear threshold outflow process: missing the 'threshold' parameter.");
        return false;
    }

    return true;
}

void ProcessOutflowLinearThreshold::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _responseFactor = GetParameterValuePointer(processSettings, "response_factor_threshold");
    _threshold = GetParameterValuePointer(processSettings, "threshold");
}

const vecDouble& ProcessOutflowLinearThreshold::GetRates() {
    double excess = _container->GetContentWithChanges() - (*_threshold);
    if (excess <= 0) {
        return StoreRates({0});
    }

    return StoreRates({(*_responseFactor) * excess});
}
