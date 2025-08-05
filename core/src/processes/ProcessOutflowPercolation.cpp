#include "ProcessOutflowPercolation.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflowPercolation::ProcessOutflowPercolation(WaterContainer* container)
    : ProcessOutflow(container),
      _rate(nullptr) {}

void ProcessOutflowPercolation::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("percolation_rate", 0.1f);
}

void ProcessOutflowPercolation::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    if (HasParameter(processSettings, "percolation_rate")) {
        _rate = GetParameterValuePointer(processSettings, "percolation_rate");
    } else {
        _rate = GetParameterValuePointer(processSettings, "rate");
    }
}

vecDouble ProcessOutflowPercolation::GetRates() {
    return {*_rate};
}
