#include "ProcessOutflowLinear.h"
#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflowLinear::ProcessOutflowLinear(WaterContainer* container)
    : ProcessOutflow(container),
      m_responseFactor(nullptr)
{}

void ProcessOutflowLinear::AssignParameters(const ProcessSettings &processSettings) {
    Process::AssignParameters(processSettings);
    m_responseFactor = GetParameterValuePointer(processSettings, "responseFactor");
}

vecDouble ProcessOutflowLinear::GetChangeRates() {
    return {(*m_responseFactor) * m_container->GetContentWithChanges()};
}