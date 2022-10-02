#include "ProcessRunoffSocont.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessRunoffSocont::ProcessRunoffSocont(WaterContainer* container)
    : ProcessOutflow(container),
      m_slope(nullptr),
      m_runoffParameter(nullptr) {}

void ProcessRunoffSocont::AssignParameters(const ProcessSettings& processSettings) {
    Process::AssignParameters(processSettings);
    m_slope = GetParameterValuePointer(processSettings, "slope");
    m_runoffParameter = GetParameterValuePointer(processSettings, "runoffCoefficient");
}

vecDouble ProcessRunoffSocont::GetRates() {
    double waterDepth = m_container->GetContentWithChanges();
    double runoff = *m_runoffParameter * pow(*m_slope, 0.5) * pow(waterDepth, 5 / 3);

    return {wxMin(runoff, waterDepth)};
}
