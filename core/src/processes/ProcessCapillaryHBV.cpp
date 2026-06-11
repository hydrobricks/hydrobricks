#include "ProcessCapillaryHBV.h"

#include <algorithm>

#include "Brick.h"
#include "WaterContainer.h"

ProcessCapillaryHBV::ProcessCapillaryHBV(WaterContainer* container)
    : ProcessOutflow(container),
      _targetBrick(nullptr),
      _maxCapillaryFlux(nullptr) {}

void ProcessCapillaryHBV::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("max_capillary_flux", 0.0f);
}

bool ProcessCapillaryHBV::IsValid() const {
    if (!ProcessOutflow::IsValid()) {
        return false;
    }
    if (_targetBrick == nullptr) {
        LogError("HBV capillary process must be linked to a target brick (soil moisture).");
        return false;
    }
    if (_maxCapillaryFlux == nullptr) {
        LogError("HBV capillary process: missing the 'max_capillary_flux' parameter.");
        return false;
    }

    return true;
}

void ProcessCapillaryHBV::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _maxCapillaryFlux = GetParameterValuePointer(processSettings, "max_capillary_flux");
}

vecDouble ProcessCapillaryHBV::GetRates() {
    WaterContainer* target = _targetBrick->GetWaterContainer();
    if (target->GetMaximumCapacity() <= 0) {
        return {0};
    }

    double deficit = 1.0 - std::clamp(target->GetTargetFillingRatio(), 0.0, 1.0);

    return {(*_maxCapillaryFlux) * deficit};
}
