#include "ProcessOutflowLinear.h"
#include "Brick.h"

ProcessOutflowLinear::ProcessOutflowLinear(Brick* brick)
    : ProcessOutflow(brick),
      m_responseFactor(nullptr)
{}

void ProcessOutflowLinear::AssignParameters(const ProcessSettings &processSettings) {
    Process::AssignParameters(processSettings);
    m_responseFactor = GetParameterValuePointer(processSettings, "responseFactor");
}

vecDouble ProcessOutflowLinear::GetChangeRates() {
    return {(*m_responseFactor) * m_brick->GetContentWithChanges()};
}