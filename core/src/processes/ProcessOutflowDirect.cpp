#include "ProcessOutflowDirect.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflowDirect::ProcessOutflowDirect(WaterContainer* container)
    : ProcessOutflow(container) {}

vecDouble ProcessOutflowDirect::GetRates() {
    return {_container->GetContentWithChanges()};
}

void ProcessOutflowDirect::RegisterProcessParametersAndForcing(SettingsModel*) {
    // Nothing to register
}