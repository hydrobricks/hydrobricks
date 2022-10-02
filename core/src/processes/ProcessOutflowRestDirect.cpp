#include "ProcessOutflowRestDirect.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflowRestDirect::ProcessOutflowRestDirect(WaterContainer* container) : ProcessOutflow(container) {}

vecDouble ProcessOutflowRestDirect::GetRates() {
    return {wxMax(m_container->GetContentWithChanges() - GetSumChangeRatesOtherProcesses(), 0)};
}
