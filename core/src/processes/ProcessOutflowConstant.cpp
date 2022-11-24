#include "ProcessOutflowConstant.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflowConstant::ProcessOutflowConstant(WaterContainer* container)
    : ProcessOutflow(container),
      m_rate(nullptr) {}

void ProcessOutflowConstant::AssignParameters(const ProcessSettings& processSettings) {
    Process::AssignParameters(processSettings);
    if (HasParameter(processSettings, "percolation_rate")) {
        m_rate = GetParameterValuePointer(processSettings, "percolation_rate");
    } else {
        m_rate = GetParameterValuePointer(processSettings, "rate");
    }
}

vecDouble ProcessOutflowConstant::GetRates() {
    return {*m_rate};
}
