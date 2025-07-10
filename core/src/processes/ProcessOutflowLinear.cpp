#include "ProcessOutflowLinear.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflowLinear::ProcessOutflowLinear(WaterContainer* container)
    : ProcessOutflow(container),
      _responseFactor(nullptr) {}

void ProcessOutflowLinear::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("response_factor", 0.2f);
}

void ProcessOutflowLinear::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _responseFactor = GetParameterValuePointer(processSettings, "response_factor");
}

vecDouble ProcessOutflowLinear::GetRates() {
    return {(*_responseFactor) * _container->GetContentWithChanges()};
}
