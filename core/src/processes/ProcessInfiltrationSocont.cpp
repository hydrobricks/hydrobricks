#include "ProcessInfiltrationSocont.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessInfiltrationSocont::ProcessInfiltrationSocont(WaterContainer* container)
    : ProcessInfiltration(container) {}

void ProcessInfiltrationSocont::RegisterProcessSettings(SettingsModel*) {
    // Nothing to register
}

void ProcessInfiltrationSocont::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
}

const vecDouble& ProcessInfiltrationSocont::GetRates() {
    if (GetTargetCapacity() <= 0) {
        return StoreRates({0});
    }

    // The share of this step's input entering the soil, as a rate over the step.
    return StoreRates(
        {_container->GetContentWithChanges() * (1 - pow(GetTargetFillingRatio(), 2)) / GetTimeStepInDays()});
}
