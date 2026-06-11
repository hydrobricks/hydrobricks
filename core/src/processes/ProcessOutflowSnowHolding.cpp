#include "ProcessOutflowSnowHolding.h"

#include <algorithm>

#include "Snowpack.h"
#include "TimeMachine.h"
#include "WaterContainer.h"

ProcessOutflowSnowHolding::ProcessOutflowSnowHolding(WaterContainer* container)
    : ProcessOutflow(container),
      _waterHoldingCapacity(nullptr) {}

void ProcessOutflowSnowHolding::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("water_holding_capacity", 0.1f);
}

bool ProcessOutflowSnowHolding::IsValid() const {
    if (!ProcessOutflow::IsValid()) {
        return false;
    }
    if (_waterHoldingCapacity == nullptr) {
        LogError("Snow holding process: missing the 'water_holding_capacity' parameter.");
        return false;
    }
    if (dynamic_cast<Snowpack*>(_container->GetParentBrick()) == nullptr) {
        LogError("The outflow:snow_holding process can only be applied to a snowpack brick.");
        return false;
    }

    return true;
}

void ProcessOutflowSnowHolding::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _waterHoldingCapacity = GetParameterValuePointer(processSettings, "water_holding_capacity");
}

vecDouble ProcessOutflowSnowHolding::GetRates() {
    auto snowpack = dynamic_cast<Snowpack*>(_container->GetParentBrick());
    if (snowpack == nullptr) {
        return {0};
    }

    double swe = snowpack->GetSnowContainer()->GetContentWithChanges();
    double excess = _container->GetContentWithChanges() - (*_waterHoldingCapacity) * swe;
    if (excess <= 0) {
        return {0};
    }

    // Release the excess within the timestep
    double timeStep = (_timeMachine != nullptr) ? *_timeMachine->GetTimeStepPointer() : 1.0;

    return {excess / timeStep};
}
