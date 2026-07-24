#include "ProcessSublimationConstant.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessSublimationConstant::ProcessSublimationConstant(WaterContainer* container)
    : ProcessSublimation(container),
      _sublimationRate(nullptr) {}

void ProcessSublimationConstant::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("sublimation_rate", 0.1f);
}

bool ProcessSublimationConstant::IsValid() const {
    if (!ProcessSublimation::IsValid()) {
        return false;
    }
    if (_sublimationRate == nullptr) {
        return false;
    }

    return true;
}

void ProcessSublimationConstant::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _sublimationRate = GetParameterValuePointer(processSettings, "sublimation_rate");
}

const vecDouble& ProcessSublimationConstant::GetRates() {
    if (!_container->ContentAccessible()) {
        return StoreRates({0});
    }

    return StoreRates({static_cast<double>(*_sublimationRate)});
}
