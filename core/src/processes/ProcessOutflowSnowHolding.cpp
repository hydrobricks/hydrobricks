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
    // The snowpack can retain liquid water up to a fraction (water_holding_capacity) of the SWE.
    // Anything above that is the excess that must drain out (an absolute amount, in mm).
    double excess = _container->GetContentWithChanges() - (*_waterHoldingCapacity) * swe;
    if (excess <= 0) {
        return {0};
    }

    // GetRates() must return a *rate* (amount per unit time), not an absolute amount: the solver
    // multiplies the rate by the timestep when integrating (see WaterContainer::ApplyConstraints,
    // where content is updated as content + rate * timeStep). Dividing the excess by the timestep
    // therefore drains exactly the excess over one step, regardless of the timestep length.
    // The 1.0 fallback covers the case where no time machine is wired up (e.g. unit tests).
    double timeStep = (_timeMachine != nullptr) ? *_timeMachine->GetTimeStepPointer() : 1.0;

    return {excess / timeStep};
}
