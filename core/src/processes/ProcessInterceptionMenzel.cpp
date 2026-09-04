#include "ProcessInterceptionMenzel.h"

#include <algorithm>
#include <cmath>

#include "Brick.h"
#include "TimeMachine.h"
#include "WaterContainer.h"

ProcessInterceptionMenzel::ProcessInterceptionMenzel(WaterContainer* container)
    : ProcessOutflow(container),
      _capacity(nullptr) {}

void ProcessInterceptionMenzel::RegisterProcessSettings(SettingsModel* modelSettings) {
    // Same 'capacity' parameter as outflow:threshold, so it is a drop-in throughfall
    // process (the canopy interception capacity, alias 'ic', resolves to it).
    modelSettings->AddProcessParameter("capacity", 2.0f);
}

bool ProcessInterceptionMenzel::IsValid() const {
    if (!ProcessOutflow::IsValid()) {
        return false;
    }
    if (_capacity == nullptr) {
        LogError("Menzel interception process: missing the 'capacity' parameter.");
        return false;
    }

    return true;
}

void ProcessInterceptionMenzel::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _capacity = GetParameterValuePointer(processSettings, "capacity");
}

const vecDouble& ProcessInterceptionMenzel::GetRates() {
    double content = _container->GetContentWithChanges();
    if (content <= 0) {
        return StoreRates({0});
    }

    // The container has already absorbed this step's incoming rain (direct pass), so the
    // storage before the rain is the current content minus the incoming fluxes.
    double siMax = *_capacity;
    double rain = _container->SumIncomingFluxes();
    double prev = std::max(content - rain, 0.0);

    double retained;
    if (rain > 0 && siMax > PRECISION) {
        // Menzel (1997): only a diminishing fraction of the rain is retained as the
        // canopy fills; the rest passes through even before the capacity is reached.
        double deltaSi = (siMax - prev) * (1.0 - std::exp(-rain / siMax));
        retained = std::min(prev + std::max(deltaSi, 0.0), siMax);
    } else {
        // No rain (or no capacity): only spill anything already above the capacity.
        retained = std::min(prev, siMax);
    }

    double throughfall = std::max(content - retained, 0.0);

    // Return a rate (amount per unit time): dividing by the time step drains exactly the
    // throughfall over one step (as in ProcessOutflowThreshold). The 1.0 fallback covers
    // the case where no time machine is wired up (e.g. unit tests).
    double timeStep = (_timeMachine != nullptr) ? *_timeMachine->GetTimeStepPointer() : 1.0;

    return StoreRates({throughfall / timeStep});
}
