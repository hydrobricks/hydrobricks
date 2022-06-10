#include "ProcessOutflowDirect.h"
#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflowDirect::ProcessOutflowDirect(WaterContainer* container)
    : ProcessOutflow(container)
{}

vecDouble ProcessOutflowDirect::GetChangeRates() {
    return {m_container->GetContentWithChanges()};
}