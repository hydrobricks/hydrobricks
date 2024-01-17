#include "ProcessOutflowPercolation.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflowPercolation::ProcessOutflowPercolation(WaterContainer* container)
    : ProcessOutflow(container),
      m_rate(nullptr) {}

void ProcessOutflowPercolation::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    if (HasParameter(processSettings, "percolation_rate")) {
        m_rate = GetParameterValuePointer(processSettings, "percolation_rate");
    } else {
        m_rate = GetParameterValuePointer(processSettings, "rate");
    }
}

vecDouble ProcessOutflowPercolation::GetRates() {
    return {*m_rate};
}
