#include "ProcessInfiltrationSocont.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessInfiltrationSocont::ProcessInfiltrationSocont(WaterContainer* container)
    : ProcessInfiltration(container) {}

void ProcessInfiltrationSocont::RegisterProcessParametersAndForcing(SettingsModel*) {
    // Nothing to register
}

void ProcessInfiltrationSocont::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
}

vecDouble ProcessInfiltrationSocont::GetRates() {
    if (GetTargetCapacity() <= 0) {
        return {0};
    }

    return {m_container->GetContentWithChanges() * (1 - pow(GetTargetFillingRatio(), 2))};
}
