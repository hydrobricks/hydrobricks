#include "ProcessOutflowRestDirect.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflowRestDirect::ProcessOutflowRestDirect(WaterContainer* container)
    : ProcessOutflow(container) {}

void ProcessOutflowRestDirect::RegisterProcessParametersAndForcing(SettingsModel*) {
    // Nothing to register
}

vecDouble ProcessOutflowRestDirect::GetRates() {
    return {wxMax(_container->GetContentWithChanges() - GetSumChangeRatesOtherProcesses(), 0)};
}
