#include "ProcessOutflowLinear.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflowLinear::ProcessOutflowLinear(WaterContainer* container)
    : ProcessOutflow(container),
      _responseFactor(nullptr) {}

void ProcessOutflowLinear::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("response_factor", 0.2f);
}

void ProcessOutflowLinear::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _responseFactor = GetParameterValuePointer(processSettings, "response_factor");
}

const vecDouble& ProcessOutflowLinear::GetRates() {
    return StoreRates({(*_responseFactor) * _container->GetContentWithChanges()});
}
