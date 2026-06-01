#include "ProcessPercolationConstant.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessPercolationConstant::ProcessPercolationConstant(WaterContainer* container)
    : ProcessOutflow(container),
      _rate(nullptr) {}

void ProcessPercolationConstant::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("percolation_rate", 0.1f);
}

void ProcessPercolationConstant::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    if (HasParameter(processSettings, "percolation_rate")) {
        _rate = GetParameterValuePointer(processSettings, "percolation_rate");
    } else {
        _rate = GetParameterValuePointer(processSettings, "rate");
    }
}

vecDouble ProcessPercolationConstant::GetRates() {
    return {*_rate};
}
