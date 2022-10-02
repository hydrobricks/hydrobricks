#include "ProcessInfiltrationSocont.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessInfiltrationSocont::ProcessInfiltrationSocont(WaterContainer* container) : ProcessInfiltration(container) {}

void ProcessInfiltrationSocont::AssignParameters(const ProcessSettings& processSettings) {
    Process::AssignParameters(processSettings);
}

vecDouble ProcessInfiltrationSocont::GetRates() {
    return {m_container->GetContentWithChanges() * (1 - pow(GetTargetFillingRatio(), 2))};
}
