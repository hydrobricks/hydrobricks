#include "ProcessOutflowDirect.h"
#include "Brick.h"

ProcessOutflowDirect::ProcessOutflowDirect(Brick* brick)
    : ProcessOutflow(brick)
{}

vecDouble ProcessOutflowDirect::GetChangeRates() {
    return {m_brick->GetWaterContainer()->GetContentWithChanges()};
}