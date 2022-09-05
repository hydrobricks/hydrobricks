#include "ProcessOutflowDirect.h"
#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflowDirect::ProcessOutflowDirect(WaterContainer* container)
    : ProcessOutflow(container)
{}

vecDouble ProcessOutflowDirect::GetRates() {
    return {m_container->GetContentWithChanges()};
}