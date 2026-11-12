#include "ProcessPercolationPowerLaw.h"

#include <cmath>

#include "Brick.h"
#include "WaterContainer.h"

ProcessPercolationPowerLaw::ProcessPercolationPowerLaw(WaterContainer* container)
    : ProcessOutflow(container),
      _rate(nullptr),
      _exponent(nullptr) {}

void ProcessPercolationPowerLaw::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("percolation_rate", 0.1f);
    modelSettings->AddProcessParameter("exponent", 1.0f);
}

bool ProcessPercolationPowerLaw::IsValid() const {
    if (!ProcessOutflow::IsValid()) {
        return false;
    }
    if (!_container->HasMaximumCapacity()) {
        LogError("Power-law percolation process: the store needs a maximum capacity.");
        return false;
    }
    if (_rate == nullptr || _exponent == nullptr) {
        LogError("Power-law percolation process: missing the 'percolation_rate' or 'exponent' parameter.");
        return false;
    }

    return true;
}

void ProcessPercolationPowerLaw::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _rate = GetParameterValuePointer(processSettings, "percolation_rate");
    _exponent = GetParameterValuePointer(processSettings, "exponent");
}

const vecDouble& ProcessPercolationPowerLaw::GetRates() {
    double ratio = _container->GetTargetFillingRatio();
    return StoreRates({*_rate * pow(ratio, static_cast<double>(*_exponent))});
}
