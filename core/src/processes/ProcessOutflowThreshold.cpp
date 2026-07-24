#include "ProcessOutflowThreshold.h"

#include "TimeMachine.h"
#include "WaterContainer.h"

ProcessOutflowThreshold::ProcessOutflowThreshold(WaterContainer* container)
    : ProcessOutflow(container),
      _capacity(nullptr) {}

void ProcessOutflowThreshold::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("capacity", 2.0f);
}

bool ProcessOutflowThreshold::IsValid() const {
    if (!ProcessOutflow::IsValid()) {
        return false;
    }
    if (_capacity == nullptr) {
        LogError("Threshold outflow process: missing the 'capacity' parameter.");
        return false;
    }

    return true;
}

void ProcessOutflowThreshold::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _capacity = GetParameterValuePointer(processSettings, "capacity");
}

const vecDouble& ProcessOutflowThreshold::GetRates() {
    double excess = _container->GetContentWithChanges() - (*_capacity);
    if (excess <= 0) {
        return StoreRates({0});
    }

    // Return a rate (amount per unit time): dividing the excess by the time step drains exactly
    // the excess over one step (as in ProcessOutflowSnowHolding). The 1.0 fallback covers the
    // case where no time machine is wired up (e.g. unit tests).
    double timeStep = (_timeMachine != nullptr) ? *_timeMachine->GetTimeStepPointer() : 1.0;

    return StoreRates({excess / timeStep});
}
